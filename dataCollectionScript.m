cd('C:\Users\Administrator\Desktop\VSCode\Buck_Converter\ml-hil-esp32');

projectFolder = pwd;
dataFolder = fullfile(projectFolder,'data');

if ~exist(dataFolder,'dir')
    mkdir(dataFolder);
end


healthy_esr = linspace(0.01, 0.08, 500);  % 10 healthy
degrading_esr = linspace(0.09, 0.49, 500);
faulty_esr = linspace(0.5, 5, 500);       % 10 faulty
esr_values = [healthy_esr, degrading_esr, faulty_esr];
labels = [zeros(1,500), ones(1,500)*1, ones(1, 500)*2];
for i = 1:length(esr_values)
    R_esr = esr_values(i);
    out = sim('BuckConverter');
    min_len = min(length(out.vout.Data), length(out.IL.Data));
    data = [out.vout.Data(1:min_len), out.IL.Data(1:min_len)];
    filename = fullfile(dataFolder, sprintf('esr_%.4f_label_%d.csv', R_esr, labels(i)));
    writematrix(data, filename);
    fprintf('Done: ESR=%.4f Label=%d\n', R_esr, labels(i));
end