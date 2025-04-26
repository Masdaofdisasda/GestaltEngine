import re
import sys

def patch_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read()

    # This pattern looks for AddBezierCurve and replaces it with AddBezierCubic
    content = re.sub(r'AddBezierCurve', 'AddBezierCubic', content)

    with open(file_path, 'w', encoding='utf-8') as file:
        file.write(content)

if __name__ == "__main__":
    patch_file(sys.argv[1])
