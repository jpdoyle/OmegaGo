function [win_sums, win_rates, total_games] = make_win_matrix(winner, loser, players)
% Makes a win matrix
%
% Rachel Kositsky
% May 6, 2016

p = length(players);
win_sums = zeros(p);
win_rates = zeros(p);

% Sum up the results for each win
for i = 1 : length(winner)
    win = winner{i}; % string that is the winner of the match
    los = loser{i};
    win_ind = find(strcmp(players, win));
    los_ind = find(strcmp(players, los));
    if isempty(win_ind) || isempty(los_ind)
        continue
    end
    if ~((1 <= win_ind) && (win_ind <= p) && (1 <= los_ind) && ...
            (los_ind <= p))
        %if (~(1 <= win_ind) || ~(win_ind <= p))
        error('Invalid index');
    end
    
    win_sums(win_ind, los_ind) = win_sums(win_ind, los_ind) + 1;
    if isnan(win_sums(win_ind, los_ind))
        error('NaN value in win matrix');
    end
end

% Divide to get a win rate
for i = 1 : p
    for j = i : p
        total_games = win_sums(i, j) + win_sums(j, i);
        if (total_games > 1)
            win_rates(i, j) = win_sums(i, j)/total_games;
            win_rates(j, i) = win_sums(j, i)/total_games;
        else
            win_rates(i, j) = NaN;
            win_rates(j, i) = NaN;
        end
    end
end

end