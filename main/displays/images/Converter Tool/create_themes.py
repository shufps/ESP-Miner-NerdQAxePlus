import os
import subprocess
from jinja2 import Environment, FileSystemLoader

# Function to process each theme directory and convert PNG files to C files
def process_theme(theme, rpath):
    theme_path = os.path.join("../themes", theme)
    os.chdir(theme_path)

    for screen in ["initscreen2", "miningscreen2", "portalscreen", "btcscreen", "settingsscreen", "splashscreen2"]:
        image_path = f"./Raw Images/{screen}.png"
        # Run the convert_single.py script to generate the C files for each image
        subprocess.run(["python3", f"{rpath}/convert_single.py", theme, image_path, screen], check=True)

    os.chdir(rpath)

# Function to generate a file using Jinja2 templates
def generate_file(template_file, output_file, context):
    env = Environment(
        loader=FileSystemLoader('.'),
        trim_blocks=True,
        lstrip_blocks=True
    )
    template = env.get_template(template_file)

    # Render the template with the given context
    rendered_output = template.render(context)

    # Write the rendered output to the output file
    with open(output_file, 'w') as f:
        f.write(rendered_output)

# Main function to process themes and generate necessary files
def main():
    rpath = os.path.dirname(os.path.realpath(__file__))
    os.chdir(os.path.join(rpath, "../themes"))

    # Process each theme directory and convert its images
    theme_dirs = [d for d in os.listdir(".") if os.path.isdir(d) and d != "."]
    for theme in theme_dirs:
        theme_name = os.path.basename(theme)
        if theme_name:
            print(f"Processing theme: {theme_name}")
            process_theme(theme_name, rpath)

    # Context for Jinja2 templates
    screens = ["initscreen2", "miningscreen2", "portalscreen", "btcscreen", "settingsscreen", "splashscreen2"]
    context = {
        'themes': theme_dirs,
        'screens': screens
    }

    # Generate themes.h using Jinja2
    generate_file('themes.h.j2', '../themes/themes.h', context)

    # Generate themes.c using Jinja2
    generate_file('themes.c.j2', '../themes/themes.c', context)

if __name__ == "__main__":
    main()
