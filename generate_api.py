import re
import os

def parse_inc_file(filepath):
    components = {}
    current_component = None
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('BEGIN_COMPONENT('):
                match = re.match(r'BEGIN_COMPONENT\((\w+),\s*"([^"]+)"\)', line)
                if match:
                    current_component = match.group(1)
                    components[current_component] = {'properties': [], 'methods': []}
            elif line.startswith('PROPERTY(') and current_component:
                match = re.match(r'PROPERTY\("([^"]+)",', line)
                if match:
                    components[current_component]['properties'].append(match.group(1))
            elif line.startswith('METHOD(') and current_component:
                match = re.match(r'METHOD\("([^"]+)",', line)
                if match:
                    components[current_component]['methods'].append(match.group(1))
            elif line.startswith('END_COMPONENT()'):
                current_component = None
    return components

def parse_system_inc_file(filepath):
    systems = {}
    current_system = None
    current_constants_group = None
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('BEGIN_SYSTEM('):
                match = re.match(r'BEGIN_SYSTEM\("([^"]+)"\)', line)
                if match:
                    current_system = match.group(1)
                    systems[current_system] = {'functions': [], 'constants': {}}
            elif line.startswith('BEGIN_CONSTANTS(') and current_system:
                match = re.match(r'BEGIN_CONSTANTS\("([^"]+)"\)', line)
                if match:
                    current_constants_group = match.group(1)
                    systems[current_system]['constants'][current_constants_group] = []
            elif line.startswith('CONSTANT(') and current_system and current_constants_group:
                match = re.match(r'CONSTANT\("([^"]+)",', line)
                if match:
                    systems[current_system]['constants'][current_constants_group].append(match.group(1))
            elif line.startswith('FUNCTION(') and current_system:
                match = re.match(r'FUNCTION\("([^"]+)",', line)
                if match:
                    systems[current_system]['functions'].append(match.group(1))
            elif line.startswith('END_CONSTANTS()'):
                current_constants_group = None
            elif line.startswith('END_SYSTEM()'):
                current_system = None
    return systems

def generate_api_lua(components, systems, output_filepath):
    with open(output_filepath, 'w') as f:
        # Write components
        for comp_name, data in components.items():
            f.write(f'---@class {comp_name}\n')
            for prop in data['properties']:
                # Infer basic types (customize as needed)
                if 'enabled' in prop.lower() or 'is' in prop.lower():
                    type_hint = 'boolean'
                elif 'position' in prop.lower() or 'scale' in prop.lower() or 'size' in prop.lower() or 'volume' in prop.lower():
                    type_hint = 'number'
                else:
                    type_hint = 'any'
                f.write(f'---@field {prop} {type_hint}\n')
            for method in data['methods']:
                f.write(f'---@field {method} fun(self: {comp_name}, ...)\n')
            f.write('\n')

        # Write systems
        for sys_name, data in systems.items():
            f.write(f'---@class {sys_name}\n')
            for func in data['functions']:
                f.write(f'---@field {func} fun(...)\n')
            for const_group, constants in data['constants'].items():
                for const in constants:
                    f.write(f'---@field {const} number\n')
            f.write('\n')

if __name__ == '__main__':
    # Relative paths from workspace root
    component_inc_file = os.path.join('Project', 'Engine', 'include', 'Script', 'LuaComponentBindings.inc')
    system_inc_file = os.path.join('Project', 'Engine', 'include', 'Script', 'LuaSystemBindings.inc')
    output_file = 'api.lua'

    components = parse_inc_file(component_inc_file)
    systems = parse_system_inc_file(system_inc_file)

    generate_api_lua(components, systems, output_file)
    print("api.lua generated successfully with components and systems.")