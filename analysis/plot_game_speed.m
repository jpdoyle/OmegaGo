% Make the game/hr/time graph

[game_times, hold_outs] = convert_game_times(dates, times);

hrs = unique(game_times);

games_per_hr = zeros(size(hrs));

for i = 1 : length(games_per_hr)
    games_per_hr(i) = numel(find(game_times == hrs(i)));
end
clear i

plot(hrs, games_per_hr, 'bx-')

title('Games played per hour over time', 'FontSize', 18);
xlabel('Date', 'FontSize', 16);
ylabel('Games played per hour', 'FontSize', 16);

% Make axis and legend fonts bigger
set(gca, 'FontSize', 14)
