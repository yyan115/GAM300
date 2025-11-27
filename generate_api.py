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

def generate_api_lua(components, output_filepath):
    with open(output_filepath, 'w') as f:
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

if __name__ == '__main__':
    # Relative paths from workspace root
    inc_file = os.path.join('Project', 'Engine', 'include', 'Script', 'LuaComponentBindings.inc')
    output_file = 'api.lua'
    components = parse_inc_file(inc_file)
    generate_api_lua(components, output_file)
    print("api.lua generated successfully.")