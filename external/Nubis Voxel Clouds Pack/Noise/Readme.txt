Asset Notes:

	1. Open Houdini 19.5 or higher (may work with older versions but this is untested)
	2. From the menu, select Assets > Install Asset Library...
	3. Select the NubisNoiseGenerator.hda file and click "Install and Create" (This will load the asset and create an instance of it in a SOP network.)

	Parameters:
	Divisions Per Axis - Voxel Grid Dimensions
	Field Names - Useful for VDB exports so that you know what each field is.  
	Tile Noise - Choose to make this tile by blending the borders.
	Border Blend Width - How far (in UVW space) to blend for tiling. 
	Export Slices - Supply a path and hit export. 
	Export VDB - Supply a path and hit export.  


Example Notes:
	There are two subfolders:
		One contains z-up tga slices of the noise texture.
		The othercontains a single VDB file of the entire field set. 