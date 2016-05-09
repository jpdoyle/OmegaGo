% Extracts the players and removes ones 

% load in the data

players = unique(black_player);
p2 = unique(white_player);
for i = 1 : length(p2)
    players{length(players)+1} = p2{i};
end

players = unique(players);

% Get rid of unwanted AIs
unwanted_AIs = {'White', 'AP-MCTS (MCTS4-NN1)', 'AP-MCTS (MCTS6-NN1)', ...
    'Neural network (1 inner layer)', 'Neural network (2 inner layers)', ...
    'AP-MCTS (MCTS4-Deep)', 'AP-MCTS (MCTS6-Deep)', ...
    'Deep Neural network (11 inner layers)'};
for i = 1 : length(unwanted_AIs)
    players(find(strcmp(players, unwanted_AIs{i}))) = [];
end

clear p2
clear i