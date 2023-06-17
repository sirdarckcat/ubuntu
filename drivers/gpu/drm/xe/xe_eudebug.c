// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 */

#include <linux/anon_inodes.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#include <drm/drm_managed.h>

#include <generated/xe_wa_oob.h>

#include "regs/xe_gt_regs.h"
#include "regs/xe_engine_regs.h"

#include "xe_assert.h"
#include "xe_device.h"
#include "xe_eudebug.h"
#include "xe_eudebug_types.h"
#include "xe_exec_queue.h"
#include "xe_force_wake.h"
#include "xe_gt.h"
#include "xe_gt_debug.h"
#include "xe_hw_engine.h"
#include "xe_lrc.h"
#include "xe_macros.h"
#include "xe_mmio.h"
#include "xe_pm.h"
#include "xe_reg_sr.h"
#include "xe_rtp.h"
#include "xe_sched_job.h"
#include "xe_vm.h"
#include "xe_wa.h"

/*
 * If there is no detected event read by userspace, during this period, assume
 * userspace problem and disconnect debugger to allow forward progress.
 */
#define XE_EUDEBUG_NO_READ_DETECTED_TIMEOUT_MS (25 * 1000)

#define for_each_debugger_rcu(debugger, head) \
	list_for_each_entry_rcu((debugger), (head), connection_link)
#define for_each_debugger(debugger, head) \
	list_for_each_entry((debugger), (head), connection_link)

#define cast_event(T, event) container_of((event), typeof(*(T)), base)

#define XE_EUDEBUG_DBG_STR "eudbg: %lld:%lu:%s (%d/%d) -> (%d/%d): "
#define XE_EUDEBUG_DBG_ARGS(d) (d)->session, \
		atomic_long_read(&(d)->events.seqno), \
		READ_ONCE(d->connection.status) <= 0 ? "disconnected" : "", \
		current->pid, \
		task_tgid_nr(current), \
		(d)->target_task->pid, \
		task_tgid_nr((d)->target_task)

#define eu_err(d, fmt, ...) drm_err(&(d)->xe->drm, XE_EUDEBUG_DBG_STR # fmt, \
				    XE_EUDEBUG_DBG_ARGS(d), ##__VA_ARGS__)
#define eu_warn(d, fmt, ...) drm_warn(&(d)->xe->drm, XE_EUDEBUG_DBG_STR # fmt, \
				      XE_EUDEBUG_DBG_ARGS(d), ##__VA_ARGS__)
#define eu_dbg(d, fmt, ...) drm_dbg(&(d)->xe->drm, XE_EUDEBUG_DBG_STR # fmt, \
				    XE_EUDEBUG_DBG_ARGS(d), ##__VA_ARGS__)

#define xe_eudebug_assert(d, ...) xe_assert((d)->xe, ##__VA_ARGS__)

#define struct_member(T, member) (((T *)0)->member)

/* Keep 1:1 parity with uapi events */
#define write_member(T_out, ptr, member, value) { \
	BUILD_BUG_ON(sizeof(*ptr) != sizeof(T_out)); \
	BUILD_BUG_ON(offsetof(typeof(*ptr), member) != \
		     offsetof(typeof(T_out), member)); \
	BUILD_BUG_ON(sizeof(ptr->member) != sizeof(value)); \
	BUILD_BUG_ON(sizeof(struct_member(T_out, member)) != sizeof(value)); \
	BUILD_BUG_ON(!typecheck(typeof((ptr)->member), value));	\
	(ptr)->member = (value); \
	}

static struct xe_eudebug_event *
event_fifo_pending(struct xe_eudebug *d)
{
	struct xe_eudebug_event *event;

	if (kfifo_peek(&d->events.fifo, &event))
		return event;

	return NULL;
}

/*
 * This is racy as we dont take the lock for read but all the
 * callsites can handle the race so we can live without lock.
 */
__no_kcsan
static unsigned int
event_fifo_num_events_peek(const struct xe_eudebug * const d)
{
	return kfifo_len(&d->events.fifo);
}

static bool
xe_eudebug_detached(struct xe_eudebug *d)
{
	int status;

	spin_lock(&d->connection.lock);
	status = d->connection.status;
	spin_unlock(&d->connection.lock);

	return status <= 0;
}

static int
xe_eudebug_error(const struct xe_eudebug * const d)
{
	const int status = READ_ONCE(d->connection.status);

	return status <= 0 ? status : 0;
}

static unsigned int
event_fifo_has_events(struct xe_eudebug *d)
{
	if (xe_eudebug_detached(d))
		return 1;

	return event_fifo_num_events_peek(d);
}

static const struct rhashtable_params rhash_res = {
	.head_offset = offsetof(struct xe_eudebug_handle, rh_head),
	.key_len = sizeof_field(struct xe_eudebug_handle, key),
	.key_offset = offsetof(struct xe_eudebug_handle, key),
	.automatic_shrinking = true,
};

static struct xe_eudebug_resource *
resource_from_type(struct xe_eudebug_resources * const res, const int t)
{
	return &res->rt[t];
}

static struct xe_eudebug_resources *
xe_eudebug_resources_alloc(void)
{
	struct xe_eudebug_resources *res;
	int err;
	int i;

	res = kzalloc(sizeof(*res), GFP_ATOMIC);
	if (!res)
		return ERR_PTR(-ENOMEM);

	mutex_init(&res->lock);

	for (i = 0; i < XE_EUDEBUG_RES_TYPE_COUNT; i++) {
		xa_init_flags(&res->rt[i].xa, XA_FLAGS_ALLOC1);
		err = rhashtable_init(&res->rt[i].rh, &rhash_res);

		if (err)
			break;
	}

	if (err) {
		while (i--) {
			xa_destroy(&res->rt[i].xa);
			rhashtable_destroy(&res->rt[i].rh);
		}

		kfree(res);
		return ERR_PTR(err);
	}

	return res;
}

static void res_free_fn(void *ptr, void *arg)
{
	XE_WARN_ON(ptr);
	kfree(ptr);
}

static void
xe_eudebug_destroy_resources(struct xe_eudebug *d)
{
	struct xe_eudebug_resources *res = d->res;
	struct xe_eudebug_handle *h;
	unsigned long j;
	int i;
	int err;

	mutex_lock(&res->lock);
	for (i = 0; i < XE_EUDEBUG_RES_TYPE_COUNT; i++) {
		struct xe_eudebug_resource *r = &res->rt[i];

		xa_for_each(&r->xa, j, h) {
			struct xe_eudebug_handle *t;

			err = rhashtable_remove_fast(&r->rh,
						     &h->rh_head,
						     rhash_res);
			xe_eudebug_assert(d, !err);
			t = xa_erase(&r->xa, h->id);
			xe_eudebug_assert(d, t == h);
			kfree(t);
		}
	}
	mutex_unlock(&res->lock);

	for (i = 0; i < XE_EUDEBUG_RES_TYPE_COUNT; i++) {
		struct xe_eudebug_resource *r = &res->rt[i];

		rhashtable_free_and_destroy(&r->rh, res_free_fn, NULL);
		xe_eudebug_assert(d, xa_empty(&r->xa));
		xa_destroy(&r->xa);
	}

	mutex_destroy(&res->lock);

	kfree(res);
}

static void xe_eudebug_free(struct kref *ref)
{
	struct xe_eudebug *d = container_of(ref, typeof(*d), ref);
	struct xe_eudebug_event *event;

	while (kfifo_get(&d->events.fifo, &event))
		kfree(event);

	xe_eudebug_destroy_resources(d);
	put_task_struct(d->target_task);

	xe_eudebug_assert(d, !kfifo_len(&d->events.fifo));

	kfree_rcu(d, rcu);
}

static void xe_eudebug_put(struct xe_eudebug *d)
{
	kref_put(&d->ref, xe_eudebug_free);
}

static struct task_struct *find_get_target(const pid_t nr)
{
	struct task_struct *task;

	rcu_read_lock();
	task = pid_task(find_pid_ns(nr, task_active_pid_ns(current)), PIDTYPE_PID);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	return task;
}

static int
xe_eudebug_attach(struct xe_device *xe, struct xe_eudebug *d,
		  const pid_t pid_nr)
{
	struct task_struct *target;
	struct xe_eudebug *iter;
	int ret = 0;

	target = find_get_target(pid_nr);
	if (!target)
		return -ENOENT;

	if (!ptrace_may_access(target, PTRACE_MODE_READ_REALCREDS)) {
		put_task_struct(target);
		return -EACCES;
	}

	XE_WARN_ON(d->connection.status != 0);

	spin_lock(&xe->eudebug.lock);
	for_each_debugger(iter, &xe->eudebug.list) {
		if (!same_thread_group(iter->target_task, target))
			continue;

		ret = -EBUSY;
	}

	if (!ret && xe->eudebug.session_count + 1 == 0)
		ret = -ENOSPC;

	if (!ret) {
		d->connection.status = XE_EUDEBUG_STATUS_CONNECTED;
		d->xe = xe;
		d->target_task = get_task_struct(target);
		d->session = ++xe->eudebug.session_count;
		kref_get(&d->ref);
		list_add_tail_rcu(&d->connection_link, &xe->eudebug.list);
	}
	spin_unlock(&xe->eudebug.lock);

	put_task_struct(target);

	return ret;
}

static bool xe_eudebug_detach(struct xe_device *xe,
			      struct xe_eudebug *d,
			      const int err)
{
	bool detached = false;

	XE_WARN_ON(err > 0);

	spin_lock(&d->connection.lock);
	if (d->connection.status == XE_EUDEBUG_STATUS_CONNECTED) {
		d->connection.status = err;
		detached = true;
	}
	spin_unlock(&d->connection.lock);

	flush_work(&d->discovery_work);

	if (!detached)
		return false;

	spin_lock(&xe->eudebug.lock);
	list_del_rcu(&d->connection_link);
	spin_unlock(&xe->eudebug.lock);

	eu_dbg(d, "session %lld detached with %d", d->session, err);

	/* Our ref with the connection_link */
	xe_eudebug_put(d);

	return true;
}

static int _xe_eudebug_disconnect(struct xe_eudebug *d,
				  const int err)
{
	wake_up_all(&d->events.write_done);
	wake_up_all(&d->events.read_done);

	return xe_eudebug_detach(d->xe, d, err);
}

#define xe_eudebug_disconnect(_d, _err) ({ \
	if (_xe_eudebug_disconnect((_d), (_err))) { \
		if ((_err) == 0 || (_err) == -ETIMEDOUT) \
			eu_dbg(d, "Session closed (%d)", (_err)); \
		else \
			eu_err(d, "Session disconnected, err = %d (%s:%d)", \
			       (_err), __func__, __LINE__); \
	} \
})

static int xe_eudebug_release(struct inode *inode, struct file *file)
{
	struct xe_eudebug *d = file->private_data;

	xe_eudebug_disconnect(d, 0);
	xe_eudebug_put(d);

	return 0;
}

static __poll_t xe_eudebug_poll(struct file *file, poll_table *wait)
{
	struct xe_eudebug * const d = file->private_data;
	__poll_t ret = 0;

	poll_wait(file, &d->events.write_done, wait);

	if (xe_eudebug_detached(d)) {
		ret |= EPOLLHUP;
		if (xe_eudebug_error(d))
			ret |= EPOLLERR;
	}

	if (event_fifo_num_events_peek(d))
		ret |= EPOLLIN;

	return ret;
}

static ssize_t xe_eudebug_read(struct file *file,
			       char __user *buf,
			       size_t count,
			       loff_t *ppos)
{
	return -EINVAL;
}

static struct xe_eudebug *
xe_eudebug_for_task_get(struct xe_device *xe,
			struct task_struct *task)
{
	struct xe_eudebug *d, *iter;

	d = NULL;

	rcu_read_lock();
	for_each_debugger_rcu(iter, &xe->eudebug.list) {
		if (!same_thread_group(iter->target_task, task))
			continue;

		if (kref_get_unless_zero(&iter->ref))
			d = iter;

		break;
	}
	rcu_read_unlock();

	return d;
}

static struct task_struct *find_task_get(struct xe_file *xef)
{
	struct task_struct *task;
	struct pid *pid;

	rcu_read_lock();
	pid = rcu_dereference(xef->drm->pid);
	task = pid_task(pid, PIDTYPE_PID);
	if (task)
		get_task_struct(task);
	rcu_read_unlock();

	return task;
}

static struct xe_eudebug *
_xe_eudebug_get(struct xe_file *xef)
{
	struct task_struct *task;
	struct xe_eudebug *d;

	d = NULL;
	task = find_task_get(xef);
	if (task) {
		d = xe_eudebug_for_task_get(to_xe_device(xef->drm->minor->dev),
					    task);
		put_task_struct(task);
	}

	if (!d)
		return NULL;

	if (xe_eudebug_detached(d)) {
		xe_eudebug_put(d);
		return NULL;
	}

	return d;
}

static struct xe_eudebug *
xe_eudebug_get(struct xe_file *xef)
{
	struct xe_eudebug *d;

	lockdep_assert_held(&xef->xe->eudebug.discovery_lock);

	d = _xe_eudebug_get(xef);
	if (d) {
		if (!completion_done(&d->discovery)) {
			xe_eudebug_put(d);
			d = NULL;
		}
	}

	return d;
}

static int xe_eudebug_queue_event(struct xe_eudebug *d,
				  struct xe_eudebug_event *event)
{
	const u64 wait_jiffies = msecs_to_jiffies(1000);
	u64 last_read_detected_ts, last_head_seqno, start_ts;

	xe_eudebug_assert(d, event->len > sizeof(struct xe_eudebug_event));
	xe_eudebug_assert(d, event->type);
	xe_eudebug_assert(d, event->type != DRM_XE_EUDEBUG_EVENT_READ);

	start_ts = ktime_get();
	last_read_detected_ts = start_ts;
	last_head_seqno = 0;

	do  {
		struct xe_eudebug_event *head;
		u64 head_seqno;
		bool was_queued;

		if (xe_eudebug_detached(d))
			break;

		spin_lock(&d->events.lock);
		head = event_fifo_pending(d);
		if (head)
			head_seqno = event->seqno;
		else
			head_seqno = 0;

		was_queued = kfifo_in(&d->events.fifo, &event, 1);
		spin_unlock(&d->events.lock);

		wake_up_all(&d->events.write_done);

		if (was_queued) {
			event = NULL;
			break;
		}

		XE_WARN_ON(!head_seqno);

		/* If we detect progress, restart timeout */
		if (last_head_seqno != head_seqno)
			last_read_detected_ts = ktime_get();

		last_head_seqno = head_seqno;

		wait_event_interruptible_timeout(d->events.read_done,
						 !kfifo_is_full(&d->events.fifo),
						 wait_jiffies);

	} while (ktime_ms_delta(ktime_get(), last_read_detected_ts) <
		 XE_EUDEBUG_NO_READ_DETECTED_TIMEOUT_MS);

	if (event) {
		eu_dbg(d,
		       "event %llu queue failed (blocked %lld ms, avail %d)",
		       event ? event->seqno : 0,
		       ktime_ms_delta(ktime_get(), start_ts),
		       kfifo_avail(&d->events.fifo));

		kfree(event);

		return -ETIMEDOUT;
	}

	return 0;
}

static struct xe_eudebug_handle *
alloc_handle(const int type, const u64 key)
{
	struct xe_eudebug_handle *h;

	h = kzalloc(sizeof(*h), GFP_ATOMIC);
	if (!h)
		return NULL;

	h->key = key;

	return h;
}

static struct xe_eudebug_handle *
__find_handle(struct xe_eudebug_resource *r,
	      const u64 key)
{
	struct xe_eudebug_handle *h;

	h = rhashtable_lookup_fast(&r->rh,
				   &key,
				   rhash_res);
	return h;
}

static int find_handle(struct xe_eudebug_resources *res,
		       const int type,
		       const void *p)
{
	const u64 key = (uintptr_t)p;
	struct xe_eudebug_resource *r;
	struct xe_eudebug_handle *h;
	int id;

	if (XE_WARN_ON(!key))
		return -EINVAL;

	r = resource_from_type(res, type);

	mutex_lock(&res->lock);
	h = __find_handle(r, key);
	id = h ? h->id : -ENOENT;
	mutex_unlock(&res->lock);

	return id;
}

static int _xe_eudebug_add_handle(struct xe_eudebug *d,
				  int type,
				  void *p,
				  u64 *seqno,
				  int *handle)
{
	const u64 key = (uintptr_t)p;
	struct xe_eudebug_resource *r;
	struct xe_eudebug_handle *h, *o;
	int err;

	if (XE_WARN_ON(!p))
		return -EINVAL;

	if (xe_eudebug_detached(d))
		return -ENOTCONN;

	h = alloc_handle(type, key);
	if (!h)
		return -ENOMEM;

	r = resource_from_type(d->res, type);

	mutex_lock(&d->res->lock);
	o = __find_handle(r, key);
	if (!o) {
		err = xa_alloc(&r->xa, &h->id, h, xa_limit_31b, GFP_KERNEL);

		if (h->id >= INT_MAX) {
			xa_erase(&r->xa, h->id);
			err = -ENOSPC;
		}

		if (!err)
			err = rhashtable_insert_fast(&r->rh,
						     &h->rh_head,
						     rhash_res);

		if (err) {
			xa_erase(&r->xa, h->id);
		} else {
			if (seqno)
				*seqno = atomic_long_inc_return(&d->events.seqno);
		}
	} else {
		xe_eudebug_assert(d, o->id);
		err = -EEXIST;
	}
	mutex_unlock(&d->res->lock);

	if (handle)
		*handle = o ? o->id : h->id;

	if (err) {
		kfree(h);
		XE_WARN_ON(err > 0);
		return err;
	}

	xe_eudebug_assert(d, h->id);

	return h->id;
}

static int xe_eudebug_add_handle(struct xe_eudebug *d,
				 int type,
				 void *p,
				 u64 *seqno)
{
	int ret;

	ret = _xe_eudebug_add_handle(d, type, p, seqno, NULL);
	if (ret == -EEXIST || ret == -ENOTCONN) {
		eu_dbg(d, "%d on adding %d", ret, type);
		return 0;
	}

	if (ret < 0)
		xe_eudebug_disconnect(d, ret);

	return ret;
}

static int _xe_eudebug_remove_handle(struct xe_eudebug *d, int type, void *p,
				     u64 *seqno)
{
	const u64 key = (uintptr_t)p;
	struct xe_eudebug_resource *r;
	struct xe_eudebug_handle *h, *xa_h;
	int ret;

	if (XE_WARN_ON(!key))
		return -EINVAL;

	if (xe_eudebug_detached(d))
		return -ENOTCONN;

	r = resource_from_type(d->res, type);

	mutex_lock(&d->res->lock);
	h = __find_handle(r, key);
	if (h) {
		ret = rhashtable_remove_fast(&r->rh,
					     &h->rh_head,
					     rhash_res);
		xe_eudebug_assert(d, !ret);
		xa_h = xa_erase(&r->xa, h->id);
		xe_eudebug_assert(d, xa_h == h);
		if (!ret) {
			ret = h->id;
			if (seqno)
				*seqno = atomic_long_inc_return(&d->events.seqno);
		}
	} else {
		ret = -ENOENT;
	}
	mutex_unlock(&d->res->lock);

	kfree(h);

	xe_eudebug_assert(d, ret);

	return ret;
}

static int xe_eudebug_remove_handle(struct xe_eudebug *d, int type, void *p,
				    u64 *seqno)
{
	int ret;

	ret = _xe_eudebug_remove_handle(d, type, p, seqno);
	if (ret == -ENOENT || ret == -ENOTCONN) {
		eu_dbg(d, "%d on removing %d", ret, type);
		return 0;
	}

	if (ret < 0)
		xe_eudebug_disconnect(d, ret);

	return ret;
}

static struct xe_eudebug_event *
xe_eudebug_create_event(struct xe_eudebug *d, u16 type, u64 seqno, u16 flags,
			u32 len)
{
	const u16 max_event = DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
	const u16 known_flags =
		DRM_XE_EUDEBUG_EVENT_CREATE |
		DRM_XE_EUDEBUG_EVENT_DESTROY |
		DRM_XE_EUDEBUG_EVENT_STATE_CHANGE |
		DRM_XE_EUDEBUG_EVENT_NEED_ACK;
	struct xe_eudebug_event *event;

	BUILD_BUG_ON(type > max_event);

	xe_eudebug_assert(d, type <= max_event);
	xe_eudebug_assert(d, !(~known_flags & flags));
	xe_eudebug_assert(d, len > sizeof(*event));

	event = kzalloc(len, GFP_KERNEL);
	if (!event)
		return NULL;

	event->len = len;
	event->type = type;
	event->flags = flags;
	event->seqno = seqno;

	return event;
}

static long xe_eudebug_read_event(struct xe_eudebug *d,
				  const u64 arg,
				  const bool wait)
{
	struct xe_device *xe = d->xe;
	struct drm_xe_eudebug_event __user * const user_orig =
		u64_to_user_ptr(arg);
	struct drm_xe_eudebug_event user_event;
	struct xe_eudebug_event *event;
	const unsigned int max_event = DRM_XE_EUDEBUG_EVENT_EU_ATTENTION;
	long ret = 0;

	if (XE_IOCTL_DBG(xe, copy_from_user(&user_event, user_orig, sizeof(user_event))))
		return -EFAULT;

	if (XE_IOCTL_DBG(xe, !user_event.type))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, user_event.type > max_event))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, user_event.type != DRM_XE_EUDEBUG_EVENT_READ))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, user_event.len < sizeof(*user_orig)))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, user_event.flags))
		return -EINVAL;

	if (XE_IOCTL_DBG(xe, user_event.reserved))
		return -EINVAL;

	/* XXX: define wait time in connect arguments ? */
	if (wait) {
		ret = wait_event_interruptible_timeout(d->events.write_done,
						       event_fifo_has_events(d),
						       msecs_to_jiffies(5 * 1000));

		if (XE_IOCTL_DBG(xe, ret < 0))
			return ret;
	}

	ret = 0;
	spin_lock(&d->events.lock);
	event = event_fifo_pending(d);
	if (event) {
		if (user_event.len < event->len) {
			ret = -EMSGSIZE;
		} else if (!kfifo_out(&d->events.fifo, &event, 1)) {
			eu_warn(d, "internal fifo corruption");
			ret = -ENOTCONN;
		}
	}
	spin_unlock(&d->events.lock);

	wake_up_all(&d->events.read_done);

	if (ret == -EMSGSIZE && put_user(event->len, &user_orig->len))
		ret = -EFAULT;

	if (XE_IOCTL_DBG(xe, ret))
		return ret;

	if (!event) {
		if (xe_eudebug_detached(d))
			return -ENOTCONN;
		if (!wait)
			return -EAGAIN;

		return -ENOENT;
	}

	if (copy_to_user(user_orig, event, event->len))
		ret = -EFAULT;
	else
		eu_dbg(d, "event read: type=%u, flags=0x%x, seqno=%llu", event->type,
		       event->flags, event->seqno);

	kfree(event);

	return ret;
}

static long xe_eudebug_ioctl(struct file *file,
			     unsigned int cmd,
			     unsigned long arg)
{
	struct xe_eudebug * const d = file->private_data;
	long ret;

	if (cmd != DRM_XE_EUDEBUG_IOCTL_READ_EVENT &&
	    !completion_done(&d->discovery))
		return -EBUSY;

	switch (cmd) {
	case DRM_XE_EUDEBUG_IOCTL_READ_EVENT:
		ret = xe_eudebug_read_event(d, arg,
					    !(file->f_flags & O_NONBLOCK));
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.release	= xe_eudebug_release,
	.poll		= xe_eudebug_poll,
	.read		= xe_eudebug_read,
	.unlocked_ioctl	= xe_eudebug_ioctl,
};

static int __current_lrca(struct xe_hw_engine *hwe, u32 *lrc_hw)
{
	u32 lrc_reg;

	lrc_reg = xe_hw_engine_mmio_read32(hwe, RING_CURRENT_LRCA(0));

	if (!(lrc_reg & CURRENT_LRCA_VALID))
		return -ENOENT;

	*lrc_hw = lrc_reg & GENMASK(31, 12);

	return 0;
}

static int current_lrca(struct xe_hw_engine *hwe, u32 *lrc_hw)
{
	unsigned int fw_ref;
	int ret;

	fw_ref = xe_force_wake_get(gt_to_fw(hwe->gt), hwe->domain);
	if (!fw_ref)
		return -ETIMEDOUT;

	ret = __current_lrca(hwe, lrc_hw);

	xe_force_wake_put(gt_to_fw(hwe->gt), fw_ref);

	return ret;
}

static bool lrca_equals(u32 a, u32 b)
{
	return (a & GENMASK(31, 12)) == (b & GENMASK(31, 12));
}

static int match_exec_queue_lrca(struct xe_exec_queue *q, u32 lrc_hw)
{
	int i;

	for (i = 0; i < q->width; i++)
		if (lrca_equals(lower_32_bits(xe_lrc_descriptor(q->lrc[i])), lrc_hw))
			return i;

	return -1;
}

static int rcu_debug1_engine_index(const struct xe_hw_engine * const hwe)
{
	if (hwe->class == XE_ENGINE_CLASS_RENDER) {
		XE_WARN_ON(hwe->instance);
		return 0;
	}

	XE_WARN_ON(hwe->instance > 3);

	return hwe->instance + 1;
}

static u32 engine_status_xe1(const struct xe_hw_engine * const hwe,
			     u32 rcu_debug1)
{
	const unsigned int first = 7;
	const unsigned int incr = 3;
	const unsigned int i = rcu_debug1_engine_index(hwe);
	const unsigned int shift = first + (i * incr);

	return (rcu_debug1 >> shift) & RCU_DEBUG_1_ENGINE_STATUS;
}

static u32 engine_status_xe2(const struct xe_hw_engine * const hwe,
			     u32 rcu_debug1)
{
	const unsigned int first = 7;
	const unsigned int incr = 4;
	const unsigned int i = rcu_debug1_engine_index(hwe);
	const unsigned int shift = first + (i * incr);

	return (rcu_debug1 >> shift) & RCU_DEBUG_1_ENGINE_STATUS;
}

static u32 engine_status(const struct xe_hw_engine * const hwe,
			 u32 rcu_debug1)
{
	u32 status = 0;

	if (GRAPHICS_VER(gt_to_xe(hwe->gt)) < 20)
		status = engine_status_xe1(hwe, rcu_debug1);
	else if (GRAPHICS_VER(gt_to_xe(hwe->gt)) < 30)
		status = engine_status_xe2(hwe, rcu_debug1);
	else
		XE_WARN_ON(GRAPHICS_VER(gt_to_xe(hwe->gt)));

	return status;
}

static bool engine_is_runalone_set(const struct xe_hw_engine * const hwe,
				   u32 rcu_debug1)
{
	return engine_status(hwe, rcu_debug1) & RCU_DEBUG_1_RUNALONE_ACTIVE;
}

static bool engine_is_context_set(const struct xe_hw_engine * const hwe,
				  u32 rcu_debug1)
{
	return engine_status(hwe, rcu_debug1) & RCU_DEBUG_1_CONTEXT_ACTIVE;
}

static bool engine_has_runalone(const struct xe_hw_engine * const hwe)
{
	return hwe->class == XE_ENGINE_CLASS_RENDER ||
		hwe->class == XE_ENGINE_CLASS_COMPUTE;
}

static struct xe_hw_engine *get_runalone_active_hw_engine(struct xe_gt *gt)
{
	struct xe_hw_engine *hwe, *first = NULL;
	unsigned int num_active, id, fw_ref;
	u32 val;

	fw_ref = xe_force_wake_get(gt_to_fw(gt), XE_FW_GT);
	if (!fw_ref) {
		drm_dbg(&gt_to_xe(gt)->drm, "eudbg: runalone failed to get force wake\n");
		return NULL;
	}

	val = xe_mmio_read32(&gt->mmio, RCU_DEBUG_1);
	xe_force_wake_put(gt_to_fw(gt), fw_ref);

	drm_dbg(&gt_to_xe(gt)->drm, "eudbg: runalone RCU_DEBUG_1 = 0x%08x\n", val);

	num_active = 0;
	for_each_hw_engine(hwe, gt, id) {
		bool runalone, ctx;

		if (!engine_has_runalone(hwe))
			continue;

		runalone = engine_is_runalone_set(hwe, val);
		ctx = engine_is_context_set(hwe, val);

		drm_dbg(&gt_to_xe(gt)->drm, "eudbg: engine %s: runalone=%s, context=%s",
			hwe->name, runalone ? "active" : "inactive",
			ctx ? "active" : "inactive");

		/*
		 * On earlier gen12 the context status seems to be idle when
		 * it has raised attention. We have to omit the active bit.
		 */
		if (IS_DGFX(gt_to_xe(gt)))
			ctx = true;

		if (runalone && ctx) {
			num_active++;

			drm_dbg(&gt_to_xe(gt)->drm, "eudbg: runalone engine %s %s",
				hwe->name, first ? "selected" : "found");
			if (!first)
				first = hwe;
		}
	}

	if (num_active > 1)
		drm_err(&gt_to_xe(gt)->drm, "eudbg: %d runalone engines active!",
			num_active);

	return first;
}

static struct xe_exec_queue *runalone_active_queue_get(struct xe_gt *gt, int *lrc_idx)
{
	struct xe_device *xe = gt_to_xe(gt);
	struct xe_exec_queue *q, *found = NULL;
	struct xe_hw_engine *active;
	struct xe_file *xef;
	unsigned long i;
	int idx, err;
	u32 lrc_hw;

	active = get_runalone_active_hw_engine(gt);
	if (!active) {
		drm_dbg(&gt_to_xe(gt)->drm, "Runalone engine not found!");
		return ERR_PTR(-ENOENT);
	}

	err = current_lrca(active, &lrc_hw);
	if (err)
		return ERR_PTR(err);

	/* Take write so that we can safely check the lists */
	down_write(&xe->eudebug.discovery_lock);
	list_for_each_entry(xef, &xe->clients.list, eudebug.client_link) {
		xa_for_each(&xef->exec_queue.xa, i, q) {
			if (q->gt != gt)
				continue;

			if (q->class != active->class)
				continue;

			if (xe_exec_queue_is_idle(q))
				continue;

			idx = match_exec_queue_lrca(q, lrc_hw);
			if (idx < 0)
				continue;

			found = xe_exec_queue_get(q);

			if (lrc_idx)
				*lrc_idx = idx;

			break;
		}

		if (found)
			break;
	}
	up_write(&xe->eudebug.discovery_lock);

	if (!found)
		return ERR_PTR(-ENOENT);

	if (XE_WARN_ON(current_lrca(active, &lrc_hw)) &&
	    XE_WARN_ON(match_exec_queue_lrca(found, lrc_hw) < 0)) {
		xe_exec_queue_put(found);
		return ERR_PTR(-ENOENT);
	}

	return found;
}

static int send_attention_event(struct xe_eudebug *d, struct xe_exec_queue *q, int lrc_idx)
{
	struct xe_eudebug_event_eu_attention *ea;
	struct xe_eudebug_event *event;
	int h_c, h_queue, h_lrc;
	u32 size = xe_gt_eu_attention_bitmap_size(q->gt);
	u32 sz = struct_size(ea, bitmask, size);
	int ret;

	XE_WARN_ON(lrc_idx < 0 || lrc_idx >= q->width);

	XE_WARN_ON(!xe_exec_queue_is_debuggable(q));

	h_c = find_handle(d->res, XE_EUDEBUG_RES_TYPE_CLIENT, q->vm->xef);
	if (h_c < 0)
		return h_c;

	h_queue = find_handle(d->res, XE_EUDEBUG_RES_TYPE_EXEC_QUEUE, q);
	if (h_queue < 0)
		return h_queue;

	h_lrc = find_handle(d->res, XE_EUDEBUG_RES_TYPE_LRC, q->lrc[lrc_idx]);
	if (h_lrc < 0)
		return h_lrc;

	event = xe_eudebug_create_event(d, DRM_XE_EUDEBUG_EVENT_EU_ATTENTION, 0,
					DRM_XE_EUDEBUG_EVENT_STATE_CHANGE, sz);

	if (!event)
		return -ENOSPC;

	ea = cast_event(ea, event);
	write_member(struct drm_xe_eudebug_event_eu_attention, ea, client_handle, (u64)h_c);
	write_member(struct drm_xe_eudebug_event_eu_attention, ea, exec_queue_handle, (u64)h_queue);
	write_member(struct drm_xe_eudebug_event_eu_attention, ea, lrc_handle, (u64)h_lrc);
	write_member(struct drm_xe_eudebug_event_eu_attention, ea, bitmask_size, size);

	mutex_lock(&d->eu_lock);
	event->seqno = atomic_long_inc_return(&d->events.seqno);
	ret = xe_gt_eu_attention_bitmap(q->gt, &ea->bitmask[0], ea->bitmask_size);
	mutex_unlock(&d->eu_lock);

	if (ret)
		return ret;

	return xe_eudebug_queue_event(d, event);
}


static int xe_send_gt_attention(struct xe_gt *gt)
{
	struct xe_eudebug *d;
	struct xe_exec_queue *q;
	int ret, lrc_idx;

	if (list_empty_careful(&gt_to_xe(gt)->eudebug.list))
		return -ENOTCONN;

	q = runalone_active_queue_get(gt, &lrc_idx);
	if (IS_ERR(q))
		return PTR_ERR(q);

	if (!xe_exec_queue_is_debuggable(q)) {
		ret = -EPERM;
		goto err_exec_queue_put;
	}

	d = _xe_eudebug_get(q->vm->xef);
	if (!d) {
		ret = -ENOTCONN;
		goto err_exec_queue_put;
	}

	if (!completion_done(&d->discovery)) {
		eu_dbg(d, "discovery not yet done\n");
		ret = -EBUSY;
		goto err_eudebug_put;
	}

	ret = send_attention_event(d, q, lrc_idx);
	if (ret)
		xe_eudebug_disconnect(d, ret);

err_eudebug_put:
	xe_eudebug_put(d);
err_exec_queue_put:
	xe_exec_queue_put(q);

	return ret;
}

static int xe_eudebug_handle_gt_attention(struct xe_gt *gt)
{
	int ret;

	ret = xe_gt_eu_threads_needing_attention(gt);
	if (ret <= 0)
		return ret;

	ret = xe_send_gt_attention(gt);

	/* Discovery in progress, fake it */
	if (ret == -EBUSY)
		return 0;

	return ret;
}

#define XE_EUDEBUG_ATTENTION_INTERVAL 100
static void attention_scan_fn(struct work_struct *work)
{
	struct xe_device *xe = container_of(work, typeof(*xe), eudebug.attention_scan.work);
	long delay = msecs_to_jiffies(XE_EUDEBUG_ATTENTION_INTERVAL);
	struct xe_gt *gt;
	u8 gt_id;

	if (list_empty_careful(&xe->eudebug.list))
		delay *= 10;

	if (delay >= HZ)
		delay = round_jiffies_up_relative(delay);

	if (xe_pm_runtime_get_if_active(xe)) {
		for_each_gt(gt, xe, gt_id) {
			int ret;

			if (gt->info.type != XE_GT_TYPE_MAIN)
				continue;

			ret = xe_eudebug_handle_gt_attention(gt);
			if (ret) {
				// TODO: error capture
				drm_info(&gt_to_xe(gt)->drm,
					 "gt:%d unable to handle eu attention ret=%d\n",
					 gt_id, ret);

				xe_gt_reset_async(gt);
			}
		}

		xe_pm_runtime_put(xe);
	}

	schedule_delayed_work(&xe->eudebug.attention_scan, delay);
}

static void attention_scan_cancel(struct xe_device *xe)
{
	cancel_delayed_work_sync(&xe->eudebug.attention_scan);
}

static void attention_scan_flush(struct xe_device *xe)
{
	mod_delayed_work(system_wq, &xe->eudebug.attention_scan, 0);
}

static void discovery_work_fn(struct work_struct *work);

static int
xe_eudebug_connect(struct xe_device *xe,
		   struct drm_xe_eudebug_connect *param)
{
	const u64 known_open_flags = 0;
	unsigned long f_flags = 0;
	struct xe_eudebug *d;
	int fd, err;

	if (param->extensions)
		return -EINVAL;

	if (!param->pid)
		return -EINVAL;

	if (param->flags & ~known_open_flags)
		return -EINVAL;

	if (param->version && param->version != DRM_XE_EUDEBUG_VERSION)
		return -EINVAL;

	param->version = DRM_XE_EUDEBUG_VERSION;

	if (!xe->eudebug.available)
		return -EOPNOTSUPP;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	kref_init(&d->ref);
	spin_lock_init(&d->connection.lock);
	mutex_init(&d->eu_lock);
	init_waitqueue_head(&d->events.write_done);
	init_waitqueue_head(&d->events.read_done);
	init_completion(&d->discovery);

	spin_lock_init(&d->events.lock);
	INIT_KFIFO(d->events.fifo);
	INIT_WORK(&d->discovery_work, discovery_work_fn);

	d->res = xe_eudebug_resources_alloc();
	if (IS_ERR(d->res)) {
		err = PTR_ERR(d->res);
		goto err_free;
	}

	err = xe_eudebug_attach(xe, d, param->pid);
	if (err)
		goto err_free_res;

	fd = anon_inode_getfd("[xe_eudebug]", &fops, d, f_flags);
	if (fd < 0) {
		err = fd;
		goto err_detach;
	}

	kref_get(&d->ref);
	queue_work(xe->eudebug.ordered_wq, &d->discovery_work);
	attention_scan_flush(xe);

	eu_dbg(d, "connected session %lld", d->session);

	return fd;

err_detach:
	xe_eudebug_detach(xe, d, err);
err_free_res:
	xe_eudebug_destroy_resources(d);
err_free:
	kfree(d);

	return err;
}

int xe_eudebug_connect_ioctl(struct drm_device *dev,
			     void *data,
			     struct drm_file *file)
{
	struct xe_device *xe = to_xe_device(dev);
	struct drm_xe_eudebug_connect * const param = data;
	int ret = 0;

	ret = xe_eudebug_connect(xe, param);

	return ret;
}

static void add_sr_entry(struct xe_hw_engine *hwe,
			 struct xe_reg_mcr mcr_reg,
			 u32 mask)
{
	const struct xe_reg_sr_entry sr_entry = {
		.reg = mcr_reg.__reg,
		.clr_bits = mask,
		.set_bits = mask,
		.read_mask = mask,
	};

	xe_reg_sr_add(&hwe->reg_sr, &sr_entry, hwe->gt);
}

void xe_eudebug_init_hw_engine(struct xe_hw_engine *hwe)
{
	struct xe_gt *gt = hwe->gt;
	struct xe_device *xe = gt_to_xe(gt);

	if (!xe->eudebug.available)
		return;

	if (!xe_rtp_match_first_render_or_compute(gt, hwe))
		return;

	if (XE_WA(gt, 18022722726))
		add_sr_entry(hwe, ROW_CHICKEN, STALL_DOP_GATING_DISABLE);

	if (XE_WA(gt, 14015474168))
		add_sr_entry(hwe, ROW_CHICKEN2, XEHPC_DISABLE_BTB);

	if (xe->info.graphics_verx100 >= 1200)
		add_sr_entry(hwe, TD_CTL,
			     TD_CTL_BREAKPOINT_ENABLE |
			     TD_CTL_FORCE_THREAD_BREAKPOINT_ENABLE |
			     TD_CTL_FEH_AND_FEE_ENABLE);

	if (xe->info.graphics_verx100 >= 1250)
		add_sr_entry(hwe, TD_CTL, TD_CTL_GLOBAL_DEBUG_ENABLE);
}

void xe_eudebug_init(struct xe_device *xe)
{
	spin_lock_init(&xe->eudebug.lock);
	INIT_LIST_HEAD(&xe->eudebug.list);

	spin_lock_init(&xe->clients.lock);
	INIT_LIST_HEAD(&xe->clients.list);
	init_rwsem(&xe->eudebug.discovery_lock);
	INIT_DELAYED_WORK(&xe->eudebug.attention_scan, attention_scan_fn);

	xe->eudebug.ordered_wq = alloc_ordered_workqueue("xe-eudebug-ordered-wq", 0);
	xe->eudebug.available = !!xe->eudebug.ordered_wq;
}

void xe_eudebug_init_late(struct xe_device *xe)
{
	if (!xe->eudebug.available)
		return;

	attention_scan_flush(xe);
}

void xe_eudebug_fini(struct xe_device *xe)
{
	attention_scan_cancel(xe);
	xe_assert(xe, list_empty_careful(&xe->eudebug.list));

	if (xe->eudebug.ordered_wq)
		destroy_workqueue(xe->eudebug.ordered_wq);
}

static int send_open_event(struct xe_eudebug *d, u32 flags, const u64 handle,
			   const u64 seqno)
{
	struct xe_eudebug_event *event;
	struct xe_eudebug_event_open *eo;

	if (!handle)
		return -EINVAL;

	if (XE_WARN_ON((long)handle >= INT_MAX))
		return -EINVAL;

	event = xe_eudebug_create_event(d, DRM_XE_EUDEBUG_EVENT_OPEN, seqno,
					flags, sizeof(*eo));
	if (!event)
		return -ENOMEM;

	eo = cast_event(eo, event);

	write_member(struct drm_xe_eudebug_event_client, eo,
		     client_handle, handle);

	return xe_eudebug_queue_event(d, event);
}

static int client_create_event(struct xe_eudebug *d, struct xe_file *xef)
{
	u64 seqno;
	int ret;

	ret = xe_eudebug_add_handle(d, XE_EUDEBUG_RES_TYPE_CLIENT, xef, &seqno);
	if (ret > 0)
		ret = send_open_event(d, DRM_XE_EUDEBUG_EVENT_CREATE,
				      ret, seqno);

	return ret;
}

static int client_destroy_event(struct xe_eudebug *d, struct xe_file *xef)
{
	u64 seqno;
	int ret;

	ret = xe_eudebug_remove_handle(d, XE_EUDEBUG_RES_TYPE_CLIENT,
				       xef, &seqno);
	if (ret > 0)
		ret = send_open_event(d, DRM_XE_EUDEBUG_EVENT_DESTROY,
				      ret, seqno);

	return ret;
}

#define xe_eudebug_event_put(_d, _err) ({ \
	if ((_err)) \
		xe_eudebug_disconnect((_d), (_err)); \
	xe_eudebug_put((_d)); \
	})

void xe_eudebug_file_open(struct xe_file *xef)
{
	struct xe_eudebug *d;

	INIT_LIST_HEAD(&xef->eudebug.client_link);

	down_read(&xef->xe->eudebug.discovery_lock);

	spin_lock(&xef->xe->clients.lock);
	list_add_tail(&xef->eudebug.client_link, &xef->xe->clients.list);
	spin_unlock(&xef->xe->clients.lock);

	d = xe_eudebug_get(xef);
	if (d)
		xe_eudebug_event_put(d, client_create_event(d, xef));

	up_read(&xef->xe->eudebug.discovery_lock);
}

void xe_eudebug_file_close(struct xe_file *xef)
{
	struct xe_eudebug *d;

	down_read(&xef->xe->eudebug.discovery_lock);
	d = xe_eudebug_get(xef);
	if (d)
		xe_eudebug_event_put(d, client_destroy_event(d, xef));

	spin_lock(&xef->xe->clients.lock);
	list_del_init(&xef->eudebug.client_link);
	spin_unlock(&xef->xe->clients.lock);

	up_read(&xef->xe->eudebug.discovery_lock);
}

static int send_vm_event(struct xe_eudebug *d, u32 flags,
			 const u64 client_handle,
			 const u64 vm_handle,
			 const u64 seqno)
{
	struct xe_eudebug_event *event;
	struct xe_eudebug_event_vm *e;

	event = xe_eudebug_create_event(d, DRM_XE_EUDEBUG_EVENT_VM,
					seqno, flags, sizeof(*e));
	if (!event)
		return -ENOMEM;

	e = cast_event(e, event);

	write_member(struct drm_xe_eudebug_event_vm, e, client_handle, client_handle);
	write_member(struct drm_xe_eudebug_event_vm, e, vm_handle, vm_handle);

	return xe_eudebug_queue_event(d, event);
}

static int vm_create_event(struct xe_eudebug *d,
			   struct xe_file *xef, struct xe_vm *vm)
{
	int h_c, h_vm;
	u64 seqno;
	int ret;

	if (!xe_vm_in_lr_mode(vm))
		return 0;

	h_c = find_handle(d->res, XE_EUDEBUG_RES_TYPE_CLIENT, xef);
	if (h_c < 0)
		return h_c;

	xe_eudebug_assert(d, h_c);

	h_vm = xe_eudebug_add_handle(d, XE_EUDEBUG_RES_TYPE_VM, vm, &seqno);
	if (h_vm <= 0)
		return h_vm;

	ret = send_vm_event(d, DRM_XE_EUDEBUG_EVENT_CREATE, h_c, h_vm, seqno);

	return ret;
}

static int vm_destroy_event(struct xe_eudebug *d,
			    struct xe_file *xef, struct xe_vm *vm)
{
	int h_c, h_vm;
	u64 seqno;

	if (!xe_vm_in_lr_mode(vm))
		return 0;

	h_c = find_handle(d->res, XE_EUDEBUG_RES_TYPE_CLIENT, xef);
	if (h_c < 0) {
		XE_WARN_ON("no client found for vm");
		eu_warn(d, "no client found for vm");
		return h_c;
	}

	xe_eudebug_assert(d, h_c);

	h_vm = xe_eudebug_remove_handle(d, XE_EUDEBUG_RES_TYPE_VM, vm, &seqno);
	if (h_vm <= 0)
		return h_vm;

	return send_vm_event(d, DRM_XE_EUDEBUG_EVENT_DESTROY, h_c, h_vm, seqno);
}

void xe_eudebug_vm_create(struct xe_file *xef, struct xe_vm *vm)
{
	struct xe_eudebug *d;

	if (!xe_vm_in_lr_mode(vm))
		return;

	d = xe_eudebug_get(xef);
	if (!d)
		return;

	xe_eudebug_event_put(d, vm_create_event(d, xef, vm));
}

void xe_eudebug_vm_destroy(struct xe_file *xef, struct xe_vm *vm)
{
	struct xe_eudebug *d;

	if (!xe_vm_in_lr_mode(vm))
		return;

	d = xe_eudebug_get(xef);
	if (!d)
		return;

	xe_eudebug_event_put(d, vm_destroy_event(d, xef, vm));
}

static bool exec_queue_class_is_tracked(enum xe_engine_class class)
{
	return class == XE_ENGINE_CLASS_COMPUTE ||
		class == XE_ENGINE_CLASS_RENDER;
}

static const u16 xe_to_user_engine_class[] = {
	[XE_ENGINE_CLASS_RENDER] = DRM_XE_ENGINE_CLASS_RENDER,
	[XE_ENGINE_CLASS_COPY] = DRM_XE_ENGINE_CLASS_COPY,
	[XE_ENGINE_CLASS_VIDEO_DECODE] = DRM_XE_ENGINE_CLASS_VIDEO_DECODE,
	[XE_ENGINE_CLASS_VIDEO_ENHANCE] = DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE,
	[XE_ENGINE_CLASS_COMPUTE] = DRM_XE_ENGINE_CLASS_COMPUTE,
};

static int send_exec_queue_event(struct xe_eudebug *d, u32 flags,
				 u64 client_handle, u64 vm_handle,
				 u64 exec_queue_handle, enum xe_engine_class class,
				 u32 width, u64 *lrc_handles, u64 seqno)
{
	struct xe_eudebug_event *event;
	struct xe_eudebug_event_exec_queue *e;
	const u32 sz = struct_size(e, lrc_handle, width);
	const u32 xe_engine_class = xe_to_user_engine_class[class];

	if (!exec_queue_class_is_tracked(class))
		return -EINVAL;

	event = xe_eudebug_create_event(d, DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE,
					seqno, flags, sz);
	if (!event)
		return -ENOMEM;

	e = cast_event(e, event);

	write_member(struct drm_xe_eudebug_event_exec_queue, e, client_handle, client_handle);
	write_member(struct drm_xe_eudebug_event_exec_queue, e, vm_handle, vm_handle);
	write_member(struct drm_xe_eudebug_event_exec_queue, e, exec_queue_handle,
		     exec_queue_handle);
	write_member(struct drm_xe_eudebug_event_exec_queue, e, engine_class, xe_engine_class);
	write_member(struct drm_xe_eudebug_event_exec_queue, e, width, width);

	memcpy(e->lrc_handle, lrc_handles, width);

	return xe_eudebug_queue_event(d, event);
}

static int send_exec_queue_placements_event(struct xe_eudebug *d,
					    u64 client_handle, u64 vm_handle,
					    u64 exec_queue_handle, u64 lrc_handle,
					    u32 num_placements, u64 *instances,
					    u64 seqno)
{
	struct xe_eudebug_event_exec_queue_placements *e;
	const u32 sz = struct_size(e, instances, num_placements);
	struct xe_eudebug_event *event;

	event = xe_eudebug_create_event(d,
					DRM_XE_EUDEBUG_EVENT_EXEC_QUEUE_PLACEMENTS,
					seqno, DRM_XE_EUDEBUG_EVENT_CREATE, sz);
	if (!event)
		return -ENOMEM;

	e = cast_event(e, event);

	write_member(struct drm_xe_eudebug_event_exec_queue_placements, e, client_handle,
		     client_handle);
	write_member(struct drm_xe_eudebug_event_exec_queue_placements, e, vm_handle, vm_handle);
	write_member(struct drm_xe_eudebug_event_exec_queue_placements, e, exec_queue_handle,
		     exec_queue_handle);
	write_member(struct drm_xe_eudebug_event_exec_queue_placements, e, lrc_handle, lrc_handle);
	write_member(struct drm_xe_eudebug_event_exec_queue_placements, e, num_placements,
		     num_placements);

	memcpy(e->instances, instances, num_placements * sizeof(*instances));

	return xe_eudebug_queue_event(d, event);
}

static int send_exec_queue_placements_events(struct xe_eudebug *d, struct xe_exec_queue *q,
					     u64 client_handle, u64 vm_handle,
					     u64 exec_queue_handle, u64 *lrc_handles)
{

	struct drm_xe_engine_class_instance eci[XE_HW_ENGINE_MAX_INSTANCE] = {};
	unsigned long mask = q->logical_mask;
	u32 num_placements = 0;
	int ret, i, j;
	u64 seqno;

	for_each_set_bit(i, &mask, sizeof(q->logical_mask) * 8) {
		if (XE_WARN_ON(num_placements == XE_HW_ENGINE_MAX_INSTANCE))
			break;

		eci[num_placements].engine_class = xe_to_user_engine_class[q->class];
		eci[num_placements].engine_instance = i;
		eci[num_placements++].gt_id = q->gt->info.id;
	}

	ret = 0;
	for (i = 0; i < q->width; i++) {
		seqno = atomic_long_inc_return(&d->events.seqno);

		ret = send_exec_queue_placements_event(d, client_handle, vm_handle,
						       exec_queue_handle, lrc_handles[i],
						       num_placements, (u64 *)eci, seqno);
		if (ret)
			return ret;

		/*
		 * Parallel submissions must be logically contiguous,
		 * so the next placement is just q->logical_mask >> 1
		 */
		for (j = 0; j < num_placements; j++) {
			eci[j].engine_instance++;
			XE_WARN_ON(eci[j].engine_instance >= XE_HW_ENGINE_MAX_INSTANCE);
		}
	}

	return ret;
}

static int exec_queue_create_events(struct xe_eudebug *d,
				    struct xe_file *xef, struct xe_exec_queue *q)
{
	int h_c, h_vm, h_queue;
	u64 h_lrc[XE_HW_ENGINE_MAX_INSTANCE], seqno;
	int i;
	int ret = 0;

	if (!xe_exec_queue_is_debuggable(q))
		return 0;

	h_c = find_handle(d->res, XE_EUDEBUG_RES_TYPE_CLIENT, xef);
	if (h_c < 0)
		return h_c;

	h_vm = find_handle(d->res, XE_EUDEBUG_RES_TYPE_VM, q->vm);
	if (h_vm < 0)
		return h_vm;

	if (XE_WARN_ON(q->width >= XE_HW_ENGINE_MAX_INSTANCE))
		return -EINVAL;

	for (i = 0; i < q->width; i++) {
		int h, ret;

		ret = _xe_eudebug_add_handle(d,
					     XE_EUDEBUG_RES_TYPE_LRC,
					     q->lrc[i],
					     NULL,
					     &h);

		if (ret < 0 && ret != -EEXIST)
			return ret;

		XE_WARN_ON(!h);

		h_lrc[i] = h;
	}

	h_queue = xe_eudebug_add_handle(d, XE_EUDEBUG_RES_TYPE_EXEC_QUEUE, q, &seqno);
	if (h_queue <= 0)
		return h_queue;

	/* No need to cleanup for added handles on error as if we fail
	 * we disconnect
	 */

	ret = send_exec_queue_event(d, DRM_XE_EUDEBUG_EVENT_CREATE,
				  h_c, h_vm, h_queue, q->class,
				  q->width, h_lrc, seqno);

	if (ret)
		return ret;

	return send_exec_queue_placements_events(d, q, h_c, h_vm, h_queue, h_lrc);
}

static int exec_queue_destroy_event(struct xe_eudebug *d,
				    struct xe_file *xef,
				    struct xe_exec_queue *q)
{
	int h_c, h_vm, h_queue;
	u64 h_lrc[XE_HW_ENGINE_MAX_INSTANCE], seqno;
	int i;

	if (!xe_exec_queue_is_debuggable(q))
		return 0;

	h_c = find_handle(d->res, XE_EUDEBUG_RES_TYPE_CLIENT, xef);
	if (h_c < 0)
		return h_c;

	h_vm = find_handle(d->res, XE_EUDEBUG_RES_TYPE_VM, q->vm);
	if (h_vm < 0)
		return h_vm;

	if (XE_WARN_ON(q->width >= XE_HW_ENGINE_MAX_INSTANCE))
		return -EINVAL;

	h_queue = xe_eudebug_remove_handle(d,
					   XE_EUDEBUG_RES_TYPE_EXEC_QUEUE,
					   q,
					   &seqno);
	if (h_queue <= 0)
		return h_queue;

	for (i = 0; i < q->width; i++) {
		int ret;

		ret = _xe_eudebug_remove_handle(d,
						XE_EUDEBUG_RES_TYPE_LRC,
						q->lrc[i],
						NULL);
		if (ret < 0 && ret != -ENOENT)
			return ret;

		XE_WARN_ON(!ret);

		h_lrc[i] = ret;
	}

	return send_exec_queue_event(d, DRM_XE_EUDEBUG_EVENT_DESTROY,
				     h_c, h_vm, h_queue, q->class,
				     q->width, h_lrc, seqno);
}

void xe_eudebug_exec_queue_create(struct xe_file *xef, struct xe_exec_queue *q)
{
	struct xe_eudebug *d;

	if (!exec_queue_class_is_tracked(q->class))
		return;

	d = xe_eudebug_get(xef);
	if (!d)
		return;

	xe_eudebug_event_put(d, exec_queue_create_events(d, xef, q));
}

void xe_eudebug_exec_queue_destroy(struct xe_file *xef, struct xe_exec_queue *q)
{
	struct xe_eudebug *d;

	if (!exec_queue_class_is_tracked(q->class))
		return;

	d = xe_eudebug_get(xef);
	if (!d)
		return;

	xe_eudebug_event_put(d, exec_queue_destroy_event(d, xef, q));
}

static int discover_client(struct xe_eudebug *d, struct xe_file *xef)
{
	struct xe_exec_queue *q;
	struct xe_vm *vm;
	unsigned long i;
	int err;

	err = client_create_event(d, xef);
	if (err)
		return err;

	xa_for_each(&xef->vm.xa, i, vm) {
		err = vm_create_event(d, xef, vm);
		if (err)
			break;
	}

	xa_for_each(&xef->exec_queue.xa, i, q) {
		if (!exec_queue_class_is_tracked(q->class))
			continue;

		err = exec_queue_create_events(d, xef, q);
		if (err)
			break;
	}

	return err;
}

static bool xe_eudebug_task_match(struct xe_eudebug *d, struct xe_file *xef)
{
	struct task_struct *task;
	bool match;

	task = find_task_get(xef);
	if (!task)
		return false;

	match = same_thread_group(d->target_task, task);

	put_task_struct(task);

	return match;
}

static void discover_clients(struct xe_device *xe, struct xe_eudebug *d)
{
	struct xe_file *xef;
	int err;

	list_for_each_entry(xef, &xe->clients.list, eudebug.client_link) {
		if (xe_eudebug_detached(d))
			break;

		if (xe_eudebug_task_match(d, xef))
			err = discover_client(d, xef);
		else
			err = 0;

		if (err) {
			eu_dbg(d, "discover client %p: %d\n", xef, err);
			break;
		}
	}
}

static void discovery_work_fn(struct work_struct *work)
{
	struct xe_eudebug *d = container_of(work, typeof(*d),
					    discovery_work);
	struct xe_device *xe = d->xe;

	if (xe_eudebug_detached(d)) {
		complete_all(&d->discovery);
		xe_eudebug_put(d);
		return;
	}

	down_write(&xe->eudebug.discovery_lock);

	eu_dbg(d, "Discovery start for %lld\n", d->session);

	discover_clients(xe, d);

	eu_dbg(d, "Discovery end for %lld\n", d->session);

	complete_all(&d->discovery);

	up_write(&xe->eudebug.discovery_lock);

	xe_eudebug_put(d);
}
