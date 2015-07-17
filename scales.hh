namespace Scales {

	struct ScaleEntry {
		int offset;
		const char *name;
		int notes[21];
	};

	const int get_number_of_scales();
	const ScaleEntry* get_scale(int index);
	const char* get_key_text(int key);
};
