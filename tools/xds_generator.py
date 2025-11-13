#!/usr/bin/env python3
"""
XOffsetDatastructure2 Schema Generator
Generates C++ types from YAML schema definitions
"""

import yaml
import sys
import os
from pathlib import Path
from typing import List, Dict, Any
from dataclasses import dataclass

@dataclass
class Field:
    name: str
    type: str
    default: Any = None
    description: str = ""

@dataclass
class StructDef:
    name: str
    fields: List[Field]
    kind: str = "struct"

class TypeAnalyzer:
    """Analyze types and generate proper C++ representations"""
    
    BASIC_TYPES = {'int', 'float', 'double', 'bool', 'char', 'long long', 'int32_t', 'int64_t', 'uint32_t', 'uint64_t'}
    XOFFSET_TYPES = {'XString', 'XVector', 'XSet', 'XMap'}
    
    @staticmethod
    def is_basic_type(type_str: str) -> bool:
        """Check if type is a basic POD type"""
        return type_str in TypeAnalyzer.BASIC_TYPES
    
    @staticmethod
    def is_xoffset_container(type_str: str) -> bool:
        """Check if type is XOffsetDatastructure container"""
        for xtype in TypeAnalyzer.XOFFSET_TYPES:
            if type_str.startswith(xtype):
                return True
        return False
    
    @staticmethod
    def needs_allocator(type_str: str, struct_names: set = None) -> bool:
        """Check if type needs allocator in constructor"""
        if TypeAnalyzer.is_xoffset_container(type_str):
            return True
        # Custom struct types also need allocator
        if struct_names and type_str in struct_names:
            return True
        return False
    
    @staticmethod
    def get_reflection_type(type_str: str, struct_names: set) -> str:
        """Convert runtime type to reflection hint type"""
        # Basic types stay the same, but use explicit int32_t etc
        if type_str == 'int':
            return 'int32_t'
        elif type_str == 'float':
            return 'float'
        elif type_str == 'double':
            return 'double'
        elif type_str == 'bool':
            return 'bool'
        
        # User-defined struct types get ReflectionHint suffix
        if type_str in struct_names:
            return f"{type_str}ReflectionHint"
        
        # Handle XVector<T>, XMap<K,V>, etc.
        if type_str.startswith('XVector<'):
            inner = type_str[8:-1]  # Extract T from XVector<T>
            inner_reflection = TypeAnalyzer.get_reflection_type(inner, struct_names)
            return f"XVector<{inner_reflection}>"
        
        if type_str.startswith('XMap<'):
            inner = type_str[5:-1]  # Extract K,V from XMap<K,V>
            parts = TypeAnalyzer.split_template_args(inner)
            if len(parts) == 2:
                key_reflection = TypeAnalyzer.get_reflection_type(parts[0], struct_names)
                val_reflection = TypeAnalyzer.get_reflection_type(parts[1], struct_names)
                return f"XMap<{key_reflection}, {val_reflection}>"
        
        if type_str.startswith('XSet<'):
            inner = type_str[5:-1]  # Extract T from XSet<T>
            inner_reflection = TypeAnalyzer.get_reflection_type(inner, struct_names)
            return f"XSet<{inner_reflection}>"
        
        # XString stays the same
        if type_str == 'XString':
            return 'XString'
        
        return type_str
    
    @staticmethod
    def split_template_args(args_str: str) -> List[str]:
        """Split template arguments handling nested templates"""
        args = []
        current = ""
        depth = 0
        
        for char in args_str:
            if char == '<':
                depth += 1
                current += char
            elif char == '>':
                depth -= 1
                current += char
            elif char == ',' and depth == 0:
                args.append(current.strip())
                current = ""
            else:
                current += char
        
        if current.strip():
            args.append(current.strip())
        
        return args

class TypeSignatureCalculator:
    """Calculate expected type signature strings"""
    
    # Constants
    REFLECTION_SUFFIX = 'ReflectionHint'
    
    # Type sizes and alignments (64-bit platform)
    TYPE_INFO = {
        'int': (4, 4, 'i32'),
        'int32_t': (4, 4, 'i32'),
        'int64_t': (8, 8, 'i64'),
        'uint32_t': (4, 4, 'u32'),
        'uint64_t': (8, 8, 'u64'),
        'float': (4, 4, 'f32'),
        'double': (8, 8, 'f64'),
        'bool': (1, 1, 'bool'),
        'char': (1, 1, 'char'),
        'long long': (8, 8, 'i64'),
        'XString': (32, 8, 'string'),
    }
    
    @staticmethod
    def get_type_signature(type_str: str, struct_map: Dict[str, 'StructDef']) -> str:
        """Generate type signature for a type"""
        # Strip ReflectionHint suffix to get original type name
        original_type = type_str
        if type_str.endswith(TypeSignatureCalculator.REFLECTION_SUFFIX):
            original_type = type_str[:-len(TypeSignatureCalculator.REFLECTION_SUFFIX)]
        
        # Handle basic types
        if type_str in TypeSignatureCalculator.TYPE_INFO:
            size, align, sig = TypeSignatureCalculator.TYPE_INFO[type_str]
            return f"{sig}[s:{size},a:{align}]"
        
        # Handle XVector<T>
        if type_str.startswith('XVector<'):
            inner = type_str[8:-1]
            inner_sig = TypeSignatureCalculator.get_type_signature(inner, struct_map)
            return f"vector[s:32,a:8]<{inner_sig}>"
        
        # Handle XSet<T>
        if type_str.startswith('XSet<'):
            inner = type_str[5:-1]
            inner_sig = TypeSignatureCalculator.get_type_signature(inner, struct_map)
            return f"set[s:32,a:8]<{inner_sig}>"
        
        # Handle XMap<K,V>
        if type_str.startswith('XMap<'):
            inner = type_str[5:-1]
            parts = TypeAnalyzer.split_template_args(inner)
            if len(parts) == 2:
                key_sig = TypeSignatureCalculator.get_type_signature(parts[0], struct_map)
                val_sig = TypeSignatureCalculator.get_type_signature(parts[1], struct_map)
                return f"map[s:32,a:8]<{key_sig},{val_sig}>"
        
        # Handle custom struct types (check original name without ReflectionHint)
        if original_type in struct_map:
            return TypeSignatureCalculator.get_struct_signature(struct_map[original_type], struct_map)
        
        return f"unknown[{type_str}]"
    
    @staticmethod
    def calculate_field_offset(fields: List[Field], index: int, struct_map: Dict[str, 'StructDef']) -> int:
        """Calculate field offset with proper alignment"""
        if index == 0:
            return 0
        
        offset = 0
        for i in range(index):
            field = fields[i]
            field_size = TypeSignatureCalculator.get_type_size(field.type, struct_map)
            field_align = TypeSignatureCalculator.get_type_align(field.type, struct_map)
            
            # Align current offset
            offset = (offset + field_align - 1) & ~(field_align - 1)
            offset += field_size
        
        # Align for current field
        current_align = TypeSignatureCalculator.get_type_align(fields[index].type, struct_map)
        offset = (offset + current_align - 1) & ~(current_align - 1)
        
        return offset
    
    @staticmethod
    def get_type_size(type_str: str, struct_map: Dict[str, 'StructDef']) -> int:
        """Get size of a type"""
        if type_str in TypeSignatureCalculator.TYPE_INFO:
            return TypeSignatureCalculator.TYPE_INFO[type_str][0]
        if type_str.startswith(('XVector<', 'XSet<', 'XMap<')):
            return 32
        if type_str == 'XString':
            return 32
        if type_str in struct_map:
            return TypeSignatureCalculator.calculate_struct_size(struct_map[type_str], struct_map)
        return 8  # Default pointer size
    
    @staticmethod
    def get_type_align(type_str: str, struct_map: Dict[str, 'StructDef']) -> int:
        """Get alignment of a type"""
        if type_str in TypeSignatureCalculator.TYPE_INFO:
            return TypeSignatureCalculator.TYPE_INFO[type_str][1]
        if type_str.startswith(('XVector<', 'XSet<', 'XMap<')):
            return 8
        if type_str == 'XString':
            return 8
        if type_str in struct_map:
            return 8  # BASIC_ALIGNMENT
        return 8
    
    @staticmethod
    def calculate_struct_size(struct: 'StructDef', struct_map: Dict[str, 'StructDef']) -> int:
        """Calculate total struct size with alignment"""
        if not struct.fields:
            return 8  # Minimum size for alignment
        
        size = 0
        max_align = 8  # BASIC_ALIGNMENT
        
        for field in struct.fields:
            field_size = TypeSignatureCalculator.get_type_size(field.type, struct_map)
            field_align = TypeSignatureCalculator.get_type_align(field.type, struct_map)
            max_align = max(max_align, field_align)
            
            # Align current position
            size = (size + field_align - 1) & ~(field_align - 1)
            size += field_size
        
        # Final padding to struct alignment
        size = (size + max_align - 1) & ~(max_align - 1)
        return size
    
    @staticmethod
    def get_struct_signature(struct: 'StructDef', struct_map: Dict[str, 'StructDef']) -> str:
        """Generate complete struct signature"""
        size = TypeSignatureCalculator.calculate_struct_size(struct, struct_map)
        align = 8  # BASIC_ALIGNMENT
        
        # Create struct_names set once instead of recreating it in the loop
        struct_names = {s.name for s in struct_map.values()}
        
        field_sigs = []
        for i, field in enumerate(struct.fields):
            offset = TypeSignatureCalculator.calculate_field_offset(struct.fields, i, struct_map)
            # Use reflection types for signature
            field_type = TypeAnalyzer.get_reflection_type(field.type, struct_names)
            field_sig = TypeSignatureCalculator.get_type_signature(field_type, struct_map)
            field_sigs.append(f"@{offset}:{field_sig}")
        
        fields_str = ",".join(field_sigs)
        return f"struct[s:{size},a:{align}]{{{fields_str}}}"

class CodeGenerator:
    """Generate C++ code from schema"""
    
    def __init__(self, structs: List[StructDef], config: Dict[str, Any]):
        self.structs = structs
        self.config = config
        self.struct_names = {s.name for s in structs}
        self.struct_map = {s.name: s for s in structs}
    
    def generate_runtime_type(self, struct: StructDef) -> str:
        """Generate runtime type with allocator constructor"""
        lines = []
        
        # Generate struct header with alignment
        lines.append(f"struct alignas(XTypeSignature::BASIC_ALIGNMENT) {struct.name} {{")
        
        # ====================================================================
        # Constructor 1: Default constructor (allocator only)
        # ====================================================================
        allocator_fields = [f.name for f in struct.fields if TypeAnalyzer.needs_allocator(f.type, self.struct_names)]
        
        lines.append("\t// Default constructor")
        if allocator_fields:
            lines.append("\ttemplate <typename Allocator>")
            ctor_init = ", ".join([f"{name}(allocator)" for name in allocator_fields])
            lines.append(f"\t{struct.name}(Allocator allocator) : {ctor_init} {{}}")
        else:
            lines.append("\ttemplate <typename Allocator>")
            lines.append(f"\t{struct.name}(Allocator allocator) {{}}")
        
        lines.append("")
        
        # ====================================================================
        # Constructor 2: Full constructor for emplace_back (if there are fields)
        # ====================================================================
        # Check if we have any non-container fields
        non_container_fields = [
            f for f in struct.fields 
            if not TypeAnalyzer.is_xoffset_container(f.type) and f.type not in self.struct_names
        ]
        
        if non_container_fields:
            lines.append("\t// Full constructor for emplace_back")
            lines.append("\ttemplate <typename Allocator>")
            
            # Build parameter list
            params = ["Allocator allocator"]
            for field in struct.fields:
                if field.type == 'XString':
                    params.append(f"const char* {field.name}_val")
                elif TypeAnalyzer.is_xoffset_container(field.type):
                    # Skip containers in full constructor (too complex)
                    continue
                elif field.type in self.struct_names:
                    # Skip nested structs (too complex)
                    continue
                else:
                    params.append(f"{field.type} {field.name}_val")
            
            # Constructor signature
            lines.append(f"\t{struct.name}({', '.join(params)})")
            
            # Build initializer list
            init_list = []
            for field in struct.fields:
                if field.type == 'XString':
                    init_list.append(f"{field.name}({field.name}_val, allocator)")
                elif TypeAnalyzer.is_xoffset_container(field.type):
                    # Initialize container with allocator
                    init_list.append(f"{field.name}(allocator)")
                elif field.type in self.struct_names:
                    # Initialize nested struct with allocator
                    init_list.append(f"{field.name}(allocator)")
                else:
                    init_list.append(f"{field.name}({field.name}_val)")
            
            if init_list:
                lines.append(f"\t\t: {init_list[0]}")
                for init in init_list[1:]:
                    lines.append(f"\t\t, {init}")
            lines.append("\t{}")
            lines.append("")
        
        # Generate fields
        for field in struct.fields:
            default_val = ""
            if field.default is not None:
                if isinstance(field.default, bool):
                    # Handle bool: true/false
                    default_val = "{true}" if field.default else "{false}"
                elif isinstance(field.default, str):
                    # Handle char: '\0', 'A', etc.
                    if field.type == 'char' and len(field.default) <= 3:  # Single char like '\0' or 'A'
                        # Need to wrap in quotes: '\0' → {'\0'}
                        if not field.default.startswith("'"):
                            default_val = f"{{'{field.default}'}}"
                        else:
                            default_val = f"{{{field.default}}}"
                    # Could extend for string defaults in the future
                elif isinstance(field.default, (int, float)):
                    # Handle numeric types
                    if field.type == 'float':
                        default_val = f"{{{field.default}f}}"
                    else:
                        default_val = f"{{{field.default}}}"
            
            lines.append(f"\t{field.type} {field.name}{default_val};")
        
        lines.append("};")
        lines.append("")
        
        return "\n".join(lines)
    
    def generate_reflection_hint_type(self, struct: StructDef) -> str:
        """Generate reflection hint type (aggregate) for boost::pfr"""
        lines = []
        
        # Generate struct header with alignment
        lines.append(f"struct alignas(XTypeSignature::BASIC_ALIGNMENT) {struct.name}ReflectionHint {{")
        
        # Generate fields with reflection types
        for field in struct.fields:
            reflection_type = TypeAnalyzer.get_reflection_type(field.type, self.struct_names)
            lines.append(f"\t{reflection_type} {field.name};")
        
        lines.append("};")
        lines.append("")
        
        return "\n".join(lines)
    
    def generate_type_signature_comment(self, struct: StructDef) -> str:
        """Generate expected type signature as a comment for reference"""
        lines = []
        lines.append(f"// Expected type signature for {struct.name}ReflectionHint:")
        lines.append(f"// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>")
        lines.append(f"// Format: struct[s:<size>,a:<align>]{{@<offset>:<field_type>,...}}")
        lines.append(f"//")
        lines.append(f"// You can verify the actual signature by uncommenting this line:")
        lines.append(f"// #pragma message(XTypeSignature::get_XTypeSignature<{struct.name}ReflectionHint>().value)")
        lines.append("")
        return "\n".join(lines)
    
    def generate_validation(self, struct: StructDef) -> str:
        """Generate compile-time validation code"""
        lines = []
        
        # Add comment explaining the validation
        lines.append(f"// Compile-time validation for {struct.name}")
        lines.append("")
        
        # Type safety validation (NEW) - Disabled on MSVC due to Boost.PFR issues
        lines.append("// 1. Type Safety Check")
        lines.append("// Type safety verification uses Boost.PFR for recursive member checking.")
        lines.append(f"static_assert(XOffsetDatastructure2::is_xbuffer_safe<{struct.name}ReflectionHint>::value,")
        lines.append(f'              "Type safety error for {struct.name}ReflectionHint");')
        lines.append("")
        
        # Size and alignment validation (always enabled)
        lines.append("// 2. Size and Alignment Check")
        lines.append(f"static_assert(sizeof({struct.name}) == sizeof({struct.name}ReflectionHint),")
        lines.append(f'              "Size mismatch: {struct.name} runtime and reflection types must have identical size");')
        lines.append(f"static_assert(alignof({struct.name}) == alignof({struct.name}ReflectionHint),")
        lines.append(f'              "Alignment mismatch: {struct.name} runtime and reflection types must have identical alignment");')
        lines.append("")
        
        # Type signature validation (unified implementation)
        lines.append("// 3. Type Signature Check")
        lines.append("// Type signature verification uses unified Boost.PFR implementation")
        lines.append("// All compilers use lightweight tuple_element and tuple_size_v APIs")
        
        # Calculate expected type signature
        expected_sig = TypeSignatureCalculator.get_struct_signature(struct, self.struct_map)
        
        # Split long signatures for readability (>100 chars)
        if len(expected_sig) > 100:
            # Format multi-line signature
            lines.append(f"static_assert(XTypeSignature::get_XTypeSignature<{struct.name}ReflectionHint>() ==")
            
            # Split at field boundaries
            import re
            # Find all field patterns @offset:type
            parts = re.split(r'(,@\d+:)', expected_sig)
            
            # Reconstruct with proper formatting
            result_lines = []
            current_line = ""
            for part in parts:
                if part.startswith(',@'):
                    if current_line:
                        result_lines.append(current_line)
                    current_line = part[1:]  # Remove leading comma
                else:
                    current_line += part
            if current_line:
                result_lines.append(current_line)
            
            # Handle struct header separately
            if result_lines and result_lines[0].startswith('struct'):
                header_end = result_lines[0].find('{')
                if header_end > 0:
                    header = result_lines[0][:header_end+1]
                    first_field = result_lines[0][header_end+1:]
                    lines.append(f'             "{header}"')
                    if first_field:
                        lines.append(f'             "{first_field},"')
                    
                    for field_line in result_lines[1:-1]:
                        lines.append(f'             "{field_line},"')
                    
                    if len(result_lines) > 1:
                        last = result_lines[-1]
                        if last.endswith('}'):
                            lines.append(f'             "{last}",')
                        else:
                            lines.append(f'             "{last}"')
                else:
                    lines.append(f'             "{expected_sig}",')
            else:
                lines.append(f'             "{expected_sig}",')
            
            lines.append(f'              "Type signature mismatch for {struct.name}ReflectionHint");')
        else:
            # Single line signature
            lines.append(f"static_assert(XTypeSignature::get_XTypeSignature<{struct.name}ReflectionHint>() == \"{expected_sig}\",")
            lines.append(f'              "Type signature mismatch for {struct.name}ReflectionHint");')
        
        lines.append("")
        
        return "\n".join(lines)
    
    def generate_header(self, output_name: str) -> str:
        """Generate complete C++ header file"""
        lines = []
        
        # Header guard
        guard_name = f"GENERATED_{output_name.upper().replace('.', '_')}_"
        lines.append(f"#ifndef {guard_name}")
        lines.append(f"#define {guard_name}")
        lines.append("")
        
        # Includes
        lines.append('#include "xoffsetdatastructure2.hpp"')
        lines.append("")
        lines.append("using namespace XOffsetDatastructure2;")
        lines.append("")
        
        # Add comment about reflection hint types
        lines.append("// ============================================================================")
        lines.append("// Runtime Types - Used for actual data storage")
        lines.append("// ============================================================================")
        lines.append("")
        
        # Generate runtime types
        for struct in self.structs:
            lines.append(self.generate_runtime_type(struct))
        
        # Add comment
        lines.append("// ============================================================================")
        lines.append("// Reflection Hint Types - Used for compile-time type analysis")
        lines.append("// ============================================================================")
        lines.append("// These are aggregate versions of runtime types that satisfy boost::pfr")
        lines.append("// requirements for reflection. They must have identical memory layout")
        lines.append("// to their runtime counterparts.")
        lines.append("// ============================================================================")
        lines.append("")
        
        # Generate reflection hint types
        for struct in self.structs:
            lines.append(self.generate_reflection_hint_type(struct))
        
        # MSVC field registry is no longer needed - using unified Boost.PFR approach
        # The lightweight tuple_element and tuple_size_v APIs work on all platforms
        
        # Add validation section
        lines.append("// ============================================================================")
        lines.append("// Compile-Time Validation")
        lines.append("// ============================================================================")
        lines.append("")
        
        for struct in self.structs:
            lines.append(self.generate_validation(struct))
        
        # Close header guard
        lines.append(f"#endif // {guard_name}")
        lines.append("")
        
        return "\n".join(lines)

def parse_yaml_schema(yaml_file: Path) -> tuple[List[StructDef], Dict[str, Any]]:
    """Parse YAML schema file"""
    with open(yaml_file, 'r', encoding='utf-8') as f:
        data = yaml.safe_load(f)
    
    structs = []
    
    for type_def in data.get('types', []):
        fields = []
        for field_def in type_def.get('fields', []):
            field = Field(
                name=field_def['name'],
                type=field_def['type'],
                default=field_def.get('default'),
                description=field_def.get('description', '')
            )
            fields.append(field)
        
        struct = StructDef(
            name=type_def['name'],
            fields=fields,
            kind=type_def.get('type', type_def.get('kind', 'struct'))  # Support both 'type' and 'kind'
        )
        structs.append(struct)
    
    config = data.get('codegen', {})
    return structs, config

def main():
    if len(sys.argv) < 2:
        print("Usage: xds_generator.py <schema.yaml> [-o output_dir]")
        print("Example: xds_generator.py test_types.xds.yaml -o generated/")
        sys.exit(1)
    
    # Parse arguments
    yaml_file = Path(sys.argv[1])
    output_dir = Path("generated")
    
    if len(sys.argv) >= 4 and sys.argv[2] == '-o':
        output_dir = Path(sys.argv[3])
    
    if not yaml_file.exists():
        print(f"Error: Schema file not found: {yaml_file}")
        sys.exit(1)
    
    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Parse schema
    print(f"Parsing schema: {yaml_file}")
    structs, config = parse_yaml_schema(yaml_file)
    print(f"  Found {len(structs)} type(s): {', '.join(s.name for s in structs)}")
    
    # Generate code
    generator = CodeGenerator(structs, config)
    output_name = yaml_file.stem.replace('.xds', '') + '.hpp'
    output_path = output_dir / output_name
    
    print(f"Generating code: {output_path}")
    code = generator.generate_header(output_name)
    
    # Write output
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(code)
    
    print(f"[OK] Successfully generated {output_path}")
    print(f"  Runtime types: {', '.join(s.name for s in structs)}")
    print(f"  Reflection types: {', '.join(s.name + 'ReflectionHint' for s in structs)}")

if __name__ == '__main__':
    main()