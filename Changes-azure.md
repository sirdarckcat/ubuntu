Azure-specific features that got dropped:

7.2
 - UBUNTU: SAUCE: azure: dma-mapping: Add wrapper function to set dma_coherent

7.3
 - UBUNTU: SAUCE: tdx: swiotlb: check set_memory_decrypted()'s return value
   (superseded upstream by d5fd03cdd6c0 "dma: swiotlb: track pool encryption
   state and honor DMA_ATTR_CC_SHARED", which checks the same return value,
   warns, and additionally disables the pool)
 - net: mana: Route ring-buffer access through offset-based helpers
   (merged upstream as 1da1a037bc60)
 - net: mana: Fall back to scattered pages for GDMA queues
   (merged upstream as 23adfc77c22c)
 - net: mana: Add handler for sriov configure
   (merged upstream as ce6b4d3216b6)
 - net: mana: Add Interrupt Moderation support
   (merged upstream as 433f482add31)
 - net: mana: refactor mana_get_strings() and mana_get_sset_count() to use switch
   (merged upstream as d52f80731055)
 - net: mana: force full-page RX buffers via ethtool private flag
   (merged upstream as 5b5cb75fb86a)
 - net: mana: Extend RX CQE coalescing up to 8 packets
   (merged upstream as 55d20f50a221)
