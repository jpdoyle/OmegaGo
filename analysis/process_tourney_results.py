# Processes results from a Go tourney text file

# Rachel Kositsky
# May 6, 2016
import re
import csv


# Input: file object that has been opened
# Output: dictionary of dictionaries for each type of output
def process_tourney_results(tourney_file):
  d = dict()
  fieldnames =  ['dates', 'times', 'winner', 'loser',
      'winner_color', 'winner_difference',
      'black_player', 'white_player', 'black_score', 'white_score']

  for field in fieldnames:
    d[field] = dict()

  num_duplicates = 0
  initial_dict = dict()
  have_seen = 0

  for line in tourney_file:
    key = re.search('\(([a-zA-Z0-9-]+)\):',line).group(1)

    if key in d['dates']:
      have_seen = 1
      num_duplicates = num_duplicates + 1
      initial_dict = dict()
      for field in fieldnames:
        initial_dict[field] = d[field][key]
    else:
      have_seen = 0


    # date: yyyy-mm-dd
    d['dates'][key] = re.search('([0-9]{4}-[0-9]{2}-[0-9]{2})', line).group(1)

    # time: hh:mm:ss
    d['times'][key] = re.search('([0-9]{2}:[0-9]{2}:[0-9]{2})', line).group(1)

    # players: list [black_player, white_player]
    player_names = re.findall('"([^"]+)"', line)
    d['black_player'][key] = player_names[0]
    d['white_player'][key] = player_names[1]

    # scores: get black_score and white_score] if game finished, None for both otherwise
    scores = [re.search('\(B ([0-9a-zA-Z]+)', line).group(1), re.search(', W ([0-9a-zA-Z]+)\)', line).group(1)]
    if scores[0] == 'None':
      d['black_score'][key] = None
      d['white_score'][key] = None
    else:
      d['black_score'][key] = int(scores[0])
      d['white_score'][key] = int(scores[1])

    winner_diff = re.search(', ([a-zA-Z0-9+]+),', line).group(1)
    # winner color: 'B' or 'W' if game completed, None otherwise
    # winner difference: number of pts the winner won by if game completed, None otherwise
    if winner_diff == 'Crash':
      d['winner_color'][key] = None
      d['winner_difference'][key] = None
    else:
      d['winner_color'][key] = re.search('([BW])', winner_diff).group(1)
      d['winner_difference'][key] = re.search('([0-9]+)', winner_diff).group(1)

    # winner_loser: [winner_type, loser_type] if game finished, [None, None] otherwise
    if len(player_names) == 2:
      d['winner'][key] = None
      d['loser'][key] = None
    else:
      d['winner'][key] = player_names[2]
      d['loser'][key] = player_names[3]

    # Check whether you've had a duplicate entry or not
    if have_seen:
      num_overlap = 0
      for field in fieldnames:
        if d[field][key] == initial_dict[field]:
          num_overlap = num_overlap + 1
      if num_overlap == len(fieldnames):
        print 'Complete duplicate entry', key
      else:
        print 'Incomplete duplication: # overlapping fields =', num_overlap, 'ID =', key

  return [d, num_duplicates]

# Input:
#  d: dictionary output by process_tourney_results
#  csv_name: the filename of the csv to write to
# Action: create csv file and saves dictionary there
def save_to_csv(dict_data, csv_file):
    fieldnames =  ['dates', 'times', 'winner', 'loser',
      'winner_color', 'winner_difference',
      'black_player', 'white_player', 'black_score', 'white_score']

    #print 'Field names:'
    #print fieldnames
    #print 'Keys:'
    #print dict_data.keys()

    try:
        with open(csv_file, 'w') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
            writer.writeheader()
            for key in dict_data['dates'].keys():
                current_row = dict()
                for field in fieldnames:
                  current_row[field] = dict_data[field][key]
                if current_row['winner_color']:
                  # Only write if it was a completed game
                  writer.writerow(current_row)
    except IOError as (errno, strerror):
            print("I/O error({0}): {1}".format(errno, strerror))    
    return            




if __name__ == '__main__':
  file_name = '/Users/rachelkositsky/Dropbox/Homework/2015-2016/Semester_2/15-418/Final_Project/OmegaGo/server/tourney/tests-jpoyle.net.txt'
  f = open(file_name)
  [results_dictionaries, num_dup] = process_tourney_results(f)
  print 'Successful dictionary learning'
  print 'Number of duplicates: ', num_dup
  f.close()

  csv_file_name = '/Users/rachelkositsky/Dropbox/Homework/2015-2016/Semester_2/15-418/Final_Project/OmegaGo/analysis/tests-jpoyle.net.csv'
  save_to_csv(results_dictionaries, csv_file_name)
  print 'Saved to csv'

