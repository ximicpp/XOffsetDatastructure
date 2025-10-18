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

class CodeGenerator:
    """Generate C++ code from schema"""
    
    def __init__(self, structs: List[StructDef], config: Dict[str, Any]):
        self.structs = structs
        self.config = config
        self.struct_names = {s.name for s in structs}
    
    def generate_runtime_type(self, struct: StructDef) -> str:
        """Generate runtime type with allocator constructor"""
        lines = []
        
        # Generate struct header with alignment
        lines.append(f"struct alignas(XTypeSignature::BASIC_ALIGNMENT) {struct.name} {{")
        
        # Generate allocator constructor
        allocator_fields = [f.name for f in struct.fields if TypeAnalyzer.needs_allocator(f.type, self.struct_names)]
        
        if allocator_fields:
            lines.append("\ttemplate <typename Allocator>")
            ctor_init = ", ".join([f"{name}(allocator)" for name in allocator_fields])
            lines.append(f"\t{struct.name}(Allocator allocator) : {ctor_init} {{}}")
        else:
            lines.append("\ttemplate <typename Allocator>")
            lines.append(f"\t{struct.name}(Allocator allocator) {{}}")
        
        
        # Generate fields
        for field in struct.fields:
            default_val = ""
            if field.default is not None:
                if isinstance(field.default, bool):
                    default_val = "{true}" if field.default else "{false}"
                elif isinstance(field.default, (int, float)):
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
        
        # Generate runtime types
        for struct in self.structs:
            lines.append(self.generate_runtime_type(struct))
        
        # Generate reflection hint types
        for struct in self.structs:
            lines.append(self.generate_reflection_hint_type(struct))
        
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
    
    print(f"✓ Successfully generated {output_path}")
    print(f"  Runtime types: {', '.join(s.name for s in structs)}")
    print(f"  Reflection types: {', '.join(s.name + 'ReflectionHint' for s in structs)}")

if __name__ == '__main__':
    main()