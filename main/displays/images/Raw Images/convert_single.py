import sys
import os
from PIL import Image

def convert_to_16bit(image_path, output_c_file):
    # Check if the input file exists
    if not os.path.isfile(image_path):
        print(f"Error: The file '{image_path}' does not exist.")
        return

    # Extract base name without extension for use in the array and struct names
    base_name = os.path.basename(image_path).replace('.png', '')

    # Process the output path and file name
    output_c_file = os.path.normpath(output_c_file)  # Normalize the path
    output_dir = os.path.dirname(output_c_file)

    if not os.path.isdir(output_dir):
        print(f"Error: The directory '{output_dir}' does not exist.")
        return

    # Open the image
    image = Image.open(image_path)
    image = image.convert('RGBA')  # Convert image to include alpha channel

    # Image dimensions
    width, height = image.size

    # Create the array name from the output file path
    relative_path_part = os.path.splitext(os.path.basename(output_c_file))[0]

    with open(output_c_file, 'w') as f:
        # Write the header
        f.write('#include "ui.h"\n\n')
        f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
        f.write('    #define LV_ATTRIBUTE_MEM_ALIGN\n')
        f.write('#endif\n\n')

        # Write image data comment
        f.write(f'// IMAGE DATA: {os.path.basename(image_path)}\n')
        f.write(f'const LV_ATTRIBUTE_MEM_ALIGN uint8_t {relative_path_part}_data[] = {{\n')

        # Process the image pixels
        pixel_data = []
        for y in range(height):
            for x in range(width):
                r, g, b, a = image.getpixel((x, y))

                # Convert to 16-bit 565 format
                r = (r >> 3) & 0x1F    # 5 bits for red
                g = (g >> 2) & 0x3F    # 6 bits for green
                b = (b >> 3) & 0x1F    # 5 bits for blue

                # Combine into a single 16-bit value
                rgb565 = (r << 11) | (g << 5) | b

                # Split into two 8-bit values (high byte and low byte)
                high_byte = (rgb565 >> 8) & 0xFF
                low_byte = rgb565 & 0xFF

                # Append to pixel data list
                pixel_data.append(f'0x{high_byte:02X}')
                pixel_data.append(f'0x{low_byte:02X}')

        # Write the pixel data to the C file
        for i, value in enumerate(pixel_data):
            if i % 16 == 0:
                f.write('    ')
            f.write(value + ', ')
            if (i + 1) % 16 == 0:
                f.write('\n')

        # End the data array
        f.write('\n};\n\n')

        # Write the lv_img_dsc_t struct
        f.write(f'const lv_img_dsc_t {relative_path_part} = {{\n')
        f.write(f'    .header.always_zero = 0,\n')
        f.write(f'    .header.w = {width},\n')
        f.write(f'    .header.h = {height},\n')
        f.write(f'    .data_size = sizeof({relative_path_part}_data),\n')
        f.write(f'    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n')
        f.write(f'    .data = {relative_path_part}_data\n')
        f.write('};\n')

    print(f"Conversion complete! Output saved to: {output_c_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_to_16bit.py <input_image.png> <output_file.c>")
    else:
        input_image = sys.argv[1]
        output_file = sys.argv[2]

        # Convert the image and save to the specified output file
        convert_to_16bit(input_image, output_file)
