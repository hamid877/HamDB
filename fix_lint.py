import sys
import re

def fix_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    # Replace static_cast<*Plan*>(...) with dynamic_cast<*Plan*>(...)
    content = re.sub(r'static_cast<(\w*Plan\*?)>', r'dynamic_cast<\1>', content)
    # Replace static_cast<*PlanNode*>(...) with dynamic_cast<*PlanNode*>(...)
    content = re.sub(r'static_cast<(\w*PlanNode\*?)>', r'dynamic_cast<\1>', content)
    
    with open(filepath, 'w') as f:
        f.write(content)

fix_file('/home/hamid/Documents/project/HamDB/src/planner/physical_planner.cpp')
fix_file('/home/hamid/Documents/project/HamDB/src/planner/executor_factory.cpp')
