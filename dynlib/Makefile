# Mockery Makefile
#
# this is used to compile test benches for dynamic machines
#

#mocks := $(wildcard *.testbench.c)

default:
	@echo "Please use make <machine>.mock to build a test bench for the machine. ($(mocks) )"

clean:
	@rm -f *.mock

dx7.mock: hexter_src/dx7_voice.c hexter_src/dx7_voice_data.c hexter_src/dx7_voice_patches.c hexter_src/dx7_voice_render.c hexter_src/dx7_voice_tables.c hexter_src/hexter_synth.c dx7.testbench.c dx7.c libtestbench.c libtestbench.h liboscillator.c Makefile
	$(CC) -o dx7.mock -DHEXTER_DEBUG_ENGINE -D DSSP_DEBUG=0xff -D__SATAN_USES_FLOATS -g -DTHIS_IS_A_MOCKERY -DHAVE_CONFIG_H -I ./ -I ../ ../kiss_fft.c ../kiss_fftr.c hexter_src/dx7_voice.c hexter_src/dx7_voice_data.c hexter_src/dx7_voice_patches.c hexter_src/dx7_voice_render.c hexter_src/dx7_voice_tables.c hexter_src/hexter_synth.c dx7.testbench.c -lm -lrt -fsanitize=address
	$(CC) -o dx7.fx.mock -DHEXTER_DEBUG_ENGINE -D DSSP_DEBUG=0xff -D__SATAN_USES_FXP -g -DTHIS_IS_A_MOCKERY -DHAVE_CONFIG_H -I ./ -I ../ ../kiss_fft.c ../kiss_fftr.c hexter_src/dx7_voice.c hexter_src/dx7_voice_data.c hexter_src/dx7_voice_patches.c hexter_src/dx7_voice_render.c hexter_src/dx7_voice_tables.c hexter_src/hexter_synth.c dx7.testbench.c -lm -lrt -fsanitize=address

%.mock: %.testbench.c %.c libtestbench.c libtestbench.h liboscillator.c Makefile
	$(CC) -g -DTHIS_IS_A_MOCKERY -DHAVE_CONFIG_H -I ../ ../kiss_fft.c ../kiss_fftr.c -o $@ $< -lm -lrt 
