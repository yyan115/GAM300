# PlayerChain.lua Implementation TODO

- [x] Define chain states as constants (EXTENDING, TAUT, LAX, COMPLETELY_LAX)
- [x] Add configurable fields (NumberOfLinks, ChainSpeed, MaxLength, TriggerKey)
- [x] Initialize variables in Start function (currentState, chainLength, endPosition, childTransforms)
- [x] Implement ExtendChain function to simulate raycast extension over time
- [x] Implement PositionBoxes function to evenly space child boxes along the chain
- [x] Implement CheckState function to determine current chain state based on player position and chain length
- [x] Update Update function to handle extension logic, box positioning, and state checking
- [x] Add input handling for triggering chain extension
- [x] Implement PlayerChain.lua script
- [x] Replace old script content with new chain manager
- [ ] Test and adjust parameters (ChainSpeed, NumberOfLinks, etc.)
