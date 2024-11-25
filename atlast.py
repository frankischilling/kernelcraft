from PIL import Image
import os

# Directory containing individual textures
textures_dir = "assets/textures/"
output_path = "texture_atlas.png"

# Size of each texture (assumed square)
texture_size = 16  # Adjust as needed
textures_per_row = 4  # Adjust based on the number of textures

# Load textures
textures = [Image.open(os.path.join(textures_dir, f)) for f in os.listdir(textures_dir) if f.endswith('.png')]

# Calculate atlas dimensions
num_textures = len(textures)
atlas_width = textures_per_row * texture_size
atlas_height = ((num_textures + textures_per_row - 1) // textures_per_row) * texture_size

# Create an empty atlas
atlas = Image.new('RGBA', (atlas_width, atlas_height))

# Paste textures into the atlas
for idx, texture in enumerate(textures):
    x = (idx % textures_per_row) * texture_size
    y = (idx // textures_per_row) * texture_size
    atlas.paste(texture, (x, y))

# Save the texture atlas
atlas.save(output_path)
print(f"Texture atlas saved to {output_path}")
