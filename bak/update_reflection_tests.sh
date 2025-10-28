#!/bin/bash

cd /mnt/g/workspace/XOffsetDatastructure/tests

for file in test_member_iteration.cpp test_reflection_type_signature.cpp test_splice_operations.cpp test_type_introspection.cpp test_reflection_compaction.cpp test_reflection_serialization.cpp test_reflection_comparison.cpp; do
    echo "Updating $file..."
    
    # Replace __cpp_reflection check with __has_include
    sed -i 's/#if __cpp_reflection >= 202306L/#if __has_include(<experimental\/meta>)/' "$file"
    
    # Add HAS_REFLECTION define after the include
    sed -i '/#include <experimental\/meta>/a #define HAS_REFLECTION 1' "$file"
    
    # Replace remaining __cpp_reflection references
    sed -i 's/__cpp_reflection >= 202306L/HAS_REFLECTION/' "$file"
    
    # Update info message
    sed -i 's/\[INFO\] __cpp_reflection = "/[INFO] Using Clang P2996 experimental reflection/' "$file"
    sed -i 's/ << __cpp_reflection << "//' "$file"
    sed -i 's/\\n"\\n/\\n/' "$file"
done

echo "All files updated!"
