import os

# --- Configuration ---
# This script is designed to capture everything.
# Add any other text-based file extensions you want to include.
ALLOWED_TEXT_EXTENSIONS = {
    # General
    ".py", ".txt", ".md", ".json", ".html", ".css", ".js", ".xml", ".yaml", ".yml",
    ".sh", ".bat", ".ps1", ".ini", ".cfg", ".toml", ".sql",
    
    # C/C++ Project Specific
    ".c", ".h", ".cpp", ".hpp",
    
    # Common project files with specific names
    "CMakeLists.txt", "README.md", ".gitignore"
}

# -------------------

def generate_full_project_summary(start_path="."):
    """
    Walks through the entire directory and generates a complete text 
    representation of the project, including all folders and files.
    """
    output_lines = []
    
    project_root_name = os.path.basename(os.path.abspath(start_path))
    output_lines.append(f"Project Root: {project_root_name}")
    output_lines.append("=" * (len(project_root_name) + 14))
    output_lines.append("\n--- Full Directory Structure ---\n")

    files_to_process = []
    
    for root, dirs, files in os.walk(start_path, topdown=True):
        # IMPORTANT: We skip the .git directory as its contents are not 
        # human-readable source code and would clutter the output.
        if '.git' in dirs:
            dirs.remove('.git')

        dirs.sort()
        files.sort()

        relative_path = os.path.relpath(root, start_path)
        level = 0 if relative_path == "." else len(relative_path.split(os.sep))
        
        # Add directory to the tree view
        if level > 0:
            indent = "    " * (level - 1)
            output_lines.append(f"{indent}|-- {os.path.basename(root)}/")

        # Add files to the tree view and the processing list
        sub_indent = "    " * level
        for filename in files:
            output_lines.append(f"{sub_indent}|-- {filename}")
            files_to_process.append(os.path.join(root, filename))

    output_lines.append("\n\n--- All File Contents ---\n")

    # Second pass: Read and append file contents
    for file_path in files_to_process:
        # Use a consistent path separator ('/') for the header for readability
        relative_file_path = os.path.relpath(file_path, start_path).replace(os.sep, '/')
        
        header = f"--- File: {relative_file_path} ---"
        output_lines.append("=" * len(header))
        output_lines.append(header)
        output_lines.append("=" * len(header))
        
        # Check if the file (by name or extension) is considered text
        file_ext = os.path.splitext(relative_file_path)[1].lower()
        is_allowed_text = file_ext in ALLOWED_TEXT_EXTENSIONS or os.path.basename(file_path) in ALLOWED_TEXT_EXTENSIONS
        
        if is_allowed_text:
            try:
                with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                    content = f.read()
                    output_lines.append(content)
            except Exception as e:
                output_lines.append(f"\n[Error: Could not read text file. {e}]\n")
        else:
            output_lines.append("\n[Content of binary or non-text file skipped]\n")
        
        output_lines.append("\n") # Add space between files for clarity

    return "\n".join(output_lines)

# This block executes when you run the script directly
if __name__ == "__main__":
    project_path = os.getcwd()
    project_text = generate_full_project_summary(project_path)
    
    output_filename = f"{os.path.basename(project_path)}_full_project.txt"
    with open(output_filename, "w", encoding="utf-8") as f:
        f.write(project_text)
        
    print(f"✅ Full project summary has been exported to: {output_filename}")
    print("This file includes all folders and files, except for the '.git' directory.")