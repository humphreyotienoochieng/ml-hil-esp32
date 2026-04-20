cd('C:\Users\Administrator\Desktop\VSCode\Buck_Converter\ml-hil-esp32');

projectFolder = pwd;
dataFolder = fullfile(projectFolder,'data');

if ~exist(dataFolder,'dir')
    mkdir(dataFolder);
end

esr_values = linspace(0.01, 5, 20);
healthy_esr = linspace(0.01, 0.08, 10);  % 10 healthy
faulty_esr = linspace(0.5, 5, 10);       % 10 faulty
esr_values = [healthy_esr, faulty_esr];
labels = [zeros(1,10), ones(1,10)];
for i = 1:length(esr_values)
    R_esr = esr_values(i);
    out = sim('BuckConverter');
    min_len = min(length(out.vout.Data), length(out.IL.Data));
    data = [out.vout.Data(1:min_len), out.IL.Data(1:min_len)];
    filename = fullfile(dataFolder, sprintf('esr_%.4f_label_%d.csv', R_esr, labels(i)));
    writematrix(data, filename);
    fprintf('Done: ESR=%.4f Label=%d\n', R_esr, labels(i));
end