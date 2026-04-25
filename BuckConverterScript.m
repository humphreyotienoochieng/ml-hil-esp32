clear;
clc;

s = serialport("COM7",115200);
configureTerminator(s,"LF");
flush(s);

%% Case 1 Healthy
R_esr = 0.01;
out = sim('BuckConverter.slx');

v = out.vout.Data;
N = min(200,length(v));

disp("HEALTHY CASE")

for i = 1:N
    writeline(s,num2str(v(i)));
    duty = readline(s);
    disp(duty);
    pause(0.001);
end

pause(1);

%% Case 2 Faulty
R_esr = 2.0;
out = sim('BuckConverter.slx');

v = out.vout.Data;
N = min(200,length(v));

disp("FAULTY CASE")

for i = 1:N
    writeline(s,num2str(v(i)));
    duty = readline(s);
    disp(duty);
    pause(0.001);
end

clear s;