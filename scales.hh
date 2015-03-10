struct ScaleEntry {
	int offset;
	const char *name;
	int notes[21];
};

static bool scales_library_initialized = false;

static struct ScaleEntry scales_library[] = {
{ 0x0, "C- ", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x0, "C-m", {0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x0, "C#m", {0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x1, "D- ", {0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x1, "D-m", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x1, "Db ", {0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
	       0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x1, "D#m", {0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
	       0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x2, "E- ", {0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x1, 0x3, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x2, "E-m", {0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	       } }, 
{ 0x2, "Eb ", {0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x2, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x3, "F- ", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x3, "F-m", {0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x3, "F# ", {0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
	       0x1, 0x3, 0x5, 0x6, 0x8, 0xa, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x3, "F#m", {0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x4, "G- ", {0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x4, "G-m", {0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x4, "G#m", {0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
	       0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x5, "A- ", {0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x1, 0x2, 0x4, 0x6, 0x8, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x5, "A-m", {0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x5, "Ab ", {0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x1, 0x3, 0x5, 0x7, 0x8, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x6, "B- ", {0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
	       0x1, 0x3, 0x4, 0x6, 0x8, 0xa, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x6, "B-m", {0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x1, 0x2, 0x4, 0x6, 0x7, 0x9, 0xb,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x6, "Bb ", {0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x3, 0x5, 0x7, 0x9, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
{ 0x6, "Bbm", {0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
	       0x0, 0x1, 0x3, 0x5, 0x6, 0x8, 0xa,
	       0x0, 0x2, 0x4, 0x5, 0x7, 0x9, 0xb
	} }, 
};

static void initialize_scale(ScaleEntry *s) {
	for(int x = 0; x < 7; x++) {
		s->notes[x +  7] = s->notes[x % 8] + 12;
		s->notes[x + 14] = s->notes[x % 8] + 24;
	}
}

static inline void initialize_scales_library() {
	if(scales_library_initialized) return;
	
	scales_library_initialized = true;

	int k, max;

	max = sizeof(scales_library) / sizeof(ScaleEntry);

	for(k = 0; k < max; k++) {
		initialize_scale(&(scales_library[k]));
	}
}
