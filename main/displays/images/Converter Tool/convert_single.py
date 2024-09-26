import sys
import os
from PIL import Image

def convert_to_16bit(theme, image_path, screen):
    # Check if the input file exists
    if not os.path.isfile(image_path):
        print(f"Error: The file '{image_path}' does not exist.")
        return

    # Extract base name without extension for use in the array and struct names
    base_name = os.path.basename(image_path).replace('.png', '')

    # Process the output path and file name
    output_c_file = os.path.normpath(f"ui_img_{screen}_png.c")  # Normalize the path


    # Open the image
    image = Image.open(image_path)

    # Check if image has an alpha channel
    if image.mode == 'RGBA':
        use_alpha = True
        image = image.convert('RGBA')  # Ensure alpha channel is included
    else:
        use_alpha = False
        image = image.convert('RGB')  # Remove alpha channel if not present

    # Image dimensions
    width, height = image.size

    with open(output_c_file, 'w') as f:
        # Write the header
        f.write('#include "ui.h"\n\n')
        f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
        f.write('    #define LV_ATTRIBUTE_MEM_ALIGN\n')
        f.write('#endif\n\n')

        # Write image data comment
        f.write(f'// IMAGE DATA: {os.path.basename(image_path)}\n')
        f.write(f'const LV_ATTRIBUTE_MEM_ALIGN uint8_t ui_img_{theme}_{screen}_png_data[] = {{\n')

        # Process the image pixels
        pixel_data = []
        for y in range(height):
            for x in range(width):
                if use_alpha:
                    r, g, b, a = image.getpixel((x, y))
                else:
                    r, g, b = image.getpixel((x, y))

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

                # If alpha is used, append the alpha byte
                if use_alpha:
                    pixel_data.append(f'0x{a:02X}')

        # Write the pixel data to the C file
        for i, value in enumerate(pixel_data):
            if i % 16 == 0:
                f.write('    ')
            f.write(value + ', ')
            if (i + 1) % 16 == 0:
                f.write('\n')

        # End the data array
        f.write('\n};\n\n')

        # Determine the correct color format
        color_format = "LV_IMG_CF_TRUE_COLOR_ALPHA" if use_alpha else "LV_IMG_CF_TRUE_COLOR"

        # Write the lv_img_dsc_t struct
        f.write(f'const lv_img_dsc_t ui_img_{theme}_{screen}_png = {{\n')
        f.write(f'    .header.always_zero = 0,\n')
        f.write(f'    .header.w = {width},\n')
        f.write(f'    .header.h = {height},\n')
        f.write(f'    .data_size = sizeof(ui_img_{theme}_{screen}_png_data),\n')
        f.write(f'    .header.cf = {color_format},\n')
        f.write(f'    .data = ui_img_{theme}_{screen}_png_data\n')
        f.write('};\n')

    print(f"Conversion complete! Output saved to: {output_c_file}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python convert_to_16bit.py <theme> <input_image.png> <screen>")
    else:
        theme = sys.argv[1]
        input_image = sys.argv[2]
        screen = sys.argv[3]

        # Convert the image and save to the specified output file
        convert_to_16bit(theme, input_image, screen)
