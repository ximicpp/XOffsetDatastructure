# APPLEMIN checks, main 7d69d66e0f9b073cd0c1037d1dd80cae99870a91

macos 12.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 0 0 0
macos 14.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 1 1 1
ios-sim 14.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 1 warning generated.
ios 14.0: USE_PMR PMR_AVAILABLE TO_CHARS_FLOAT = 1 warning generated.

== macos-c++20
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=12.0
sample binary: 
test binaries: 0; importing std::pmr symbols: 0
== macos-c++23
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=12.0
sample binary: 
test binaries: 0; importing std::pmr symbols: 0
== ios-sim-c++20
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=14.0
sample binary: 
test binaries: 0; importing std::pmr symbols: 0
== ios-device-c++20
CMAKE_OSX_DEPLOYMENT_TARGET:STRING=14.0
sample binary: 
test binaries: 0; importing std::pmr symbols: 0
