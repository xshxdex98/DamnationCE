/*
MACOS_TAG_VALIDATE.C

The 64-bit build's stand-in for the map tag validator (port/linux/game/
tag_validate.c and tag_schema_*.c), compiled with the host's own ABI (the
game's long is 32 bits: int here; its boolean is a byte).

The validator reads the tags' Xbox addresses as pointers and lays out its
headers with them, which only the 32-bit builds can do. Until it is written
with XPTR fields, maps load here unchecked, as they did before it: the
game's own checks as it reads them still stand (tag_groups.c's empty data,
the bounded indices).
*/

unsigned char tag_validate_tags(void *tag_header, int tag_data_size, int file_length, char const *map_name)
{
	(void)tag_header;
	(void)tag_data_size;
	(void)file_length;
	(void)map_name;
	return 1;
}

unsigned char tag_validate_structure_bsp(int tag_index, void *base, int size)
{
	(void)tag_index;
	(void)base;
	(void)size;
	return 1;
}
