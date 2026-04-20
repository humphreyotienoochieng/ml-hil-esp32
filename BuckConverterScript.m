sim ('BuckConverter.slx')

plot(out.vout.Time, out.vout.Data)
ylabel("Vout (V)")
xlabel("Time (s)")
grid on

s = serialport("COM7", 115200);
configureTerminator(s, "LF");
for i = 1:200
    writeline(s, num2str(out.vout.Data(i)));
    duty = readline(s);
    disp(duty);
    pause(0.00005);
end
clear s;