% Train Time plotting

[t, hold_out] = convert_train_times(dates);
acc = correct./total;

% Get indices per layer
min_layer_num = min(layers);
max_layer_num = max(layers);
inds = cell(1, max_layer_num);
for i = min_layer_num : max_layer_num
    inds{i} = find(layers == i);
end


figure
hold on
d = cell(1, max_layer_num);
a = cell(1, max_layer_num);
for i = min_layer_num : max_layer_num
    d{i} = t(inds{i});
    a{i} = acc(inds{i});
    plot(d{i}, a{i});
end

title('Training Accuracy over Time', 'FontSize', 18);
xlabel('Time', 'FontSize', 16);
ylabel('Training Accuracy (on random batch)', 'FontSize', 16);
leg = legend('3 inner layers', '4 inner layers', '5 inner layers', ...
    '6 inner layers');
% Make axis and legend fonts bigger
set(leg, 'FontSize', 14);
set(leg, 'Location', 'northwest');
set(gca, 'FontSize', 14)
