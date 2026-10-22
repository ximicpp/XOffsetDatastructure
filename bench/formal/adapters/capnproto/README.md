# Cap’n Proto 1.5.0 experimental adapter

Native Builder state, trusted detached profile. load copies unaligned caller bytes
into a temporary aligned word array, reads with explicit traversal/nesting bounds,
then setRoot deep-copies to an independent MallocMessageBuilder. Both temporary
owners die before load returns. Bounds do not establish a full U-mutable contract.

Query uses generated native Reader views and the shared query sink, without DTO
conversion. Changes operate generated Builders. Growing samples/attrs uses native
orphans to keep the old value alive while copying into replacement lists. A full
erase stage and a full append stage each rebuild the native record list once;
ordered statuses and resulting sorted IDs remain identical to sequential reference.
Stage batching uses current state and trace operations, never expected oracle state.

The default multi-segment arena is not pre-sized for future trace values. Replaced
objects may leave retained arena occupancy; no special compact pass is hidden in
setup or emit. emit returns the native owning messageToFlatArray result directly,
including its real flattening work. Segment arrays, orphan lifetimes, temporaries
and state are all RAII-owned. The complete message is released before independent
reload as required by the shared driver.
