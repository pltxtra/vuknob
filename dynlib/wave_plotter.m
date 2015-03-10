#! /usr/bin/octave -qf

arg_list = argv ();

file = arg_list{1};
[y,Fs,bits] = wavread(file);

if (nargin == 3)
	start_i = str2num(arg_list{2}) + 1;
	stop_i = str2num(arg_list{3}) - 1;
	indexes = start_i:1:(start_i + stop_i);
      else
	indexes = 1:1:length(y);
endif
x = indexes / 44100;

hold on;
plot(x, y(indexes))

pause
