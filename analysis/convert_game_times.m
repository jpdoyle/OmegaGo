function [t, hold_out_inds] = convert_game_times(dates, times)

% Convert date format

n = length(dates);
hold_out_inds = [];

Y = 2016; % * ones(n, 1);
M = 5;
D = zeros(n, 1);
H = zeros(n, 1);
MI = 0;
S = 0;

for i = 1 : n
    curr_date = dates{i};
    curr_time = times{i};
    try
        D(i) = str2double(curr_date(9:10));
        H(i) = str2double(curr_time(1:2));
    catch
        D(i) = NaN;
        H(i) = NaN;
        hold_out_inds = [hold_out_inds i];
        disp([int2str(i) ' , ' int2str(curr_date)]);
    end
end

t = datetime(Y, M, D, H, MI, S);

end