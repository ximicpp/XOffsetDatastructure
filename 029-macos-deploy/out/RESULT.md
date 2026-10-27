# NATIVE_DEPLOY 2026-10-27T02:36:39Z, commit db5d6d19831da0c3806589bfb0483271b3bcf584
ProductName:		macOS ProductVersion:		26.3 BuildVersion:		25D125 

| case | expected | observed |
| --- | --- | --- |
| a | cache 14.0, minos 14.0, test passes | cache '14.0', minos 14.0, test passed |
| b | stops at the pmr #error | stopped at the #error |
| c | cache empty (compiler default), test passes | cache '', minos 26.0, test passed |
