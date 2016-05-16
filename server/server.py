from flask import Flask, url_for, render_template, request, \
                  redirect, make_response
import flask.json as json
from subprocess import Popen,PIPE
import multiprocessing
import uuid, re, string, random, time, os, sys
from threading import Condition,Thread,Lock
import errno
import aicontrol
from datetime import datetime,timedelta
import resource,socket
from loadbalance import WorkerLoad

mypath = sys.argv[0]
print mypath
mypath = os.path.dirname(mypath)
if mypath == '':
    mypath = '.'

serverVersion = ''
projroot = mypath + "/.."
with open(projroot+"/.git/refs/heads/master") as f:
    serverVersion = f.read().rstrip()
print "Server is at commit {}".format(serverVersion)

os.nice(20)

resource.setrlimit(resource.RLIMIT_CORE, (-1, -1))

DEBUG = True
USE_DEBUGGER = True
# Create application
app = Flask(__name__)
app.config.from_object(__name__)

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise

@app.route('/')
def hello_world(name=None):
    #return url_for('static', filename='index.html')
    return render_template('index.html', name=name)

games = {}
gameForUser = {}
userLock = Lock()
ais = []
aiLock = Condition()
tryingToStart = {}
queueLock = Condition()
workers = {}
workerLock = Lock()
runningTourney = False
nextTourneyGame = None
currTourneyGame = None

def endGame(gameId,game):
    bname,wname = game['names']
    resultVal = "Crash"
    result = "Crash"
    bscore,wscore = None,None
    if 'scores' in game['board']:
        bscore,wscore = game['board']['scores']
        result = ('B+{}'.format(bscore-wscore)) \
                if (bscore > wscore) \
                else ('W+{}'.format(wscore-bscore))
        names = [bname,wname] if bscore > wscore \
                                else [wname,bname]
        resultVal = '"{}" > "{}"'.format(*names)

    mypath = sys.argv[0]
    print mypath
    if '/' in mypath:
        mypath = mypath[:mypath.rfind('/')]
    else:
        mypath = '.'

    resultStr = '{}: "{}" vs "{}" ({}): (B {}, W {}), {}, {}' \
            .format(datetime.now(),bname,wname,gameId,bscore,
                    wscore,result,resultVal)
    mkdir_p(mypath + '/tourney')
    resultsFile = 'tourney/tests-' + socket.gethostname() + '.txt'
    with open(resultsFile,'a') as of:
        of.write(resultStr + '\n')


def refreshWorker(wid,data):
    global ais
    with workerLock:
        workers[wid] = data
        workers[wid]['lastUpdate'] = datetime.now()

        now = datetime.now()
        toRemove = []
        for w,dat in workers.iteritems():
            if now - dat['lastUpdate'] >= timedelta(seconds=30):
                toRemove.append(w)

        for w in toRemove:
            with queueLock:
                if w in tryingToStart:
                    starting = tryingToStart[w]
                    with aiLock:
                        ais = [t for tid,t in starting.iteritems()
                                if tid in gameForUser] + ais
                        aiLock.notifyAll()
            del workers[w]


def uniqueUuid(idSet):
    ret = str(uuid.uuid4())
    while ret in idSet:
        ret = str(uuid.uuid4())
    return ret;

def parseBoard(lines):
    moveState = lines[0]
    moveSearch = re.search('^Move (\d+)',moveState)
    moveNum = int(moveSearch.group(1))
    moveState = moveState[moveSearch.end():]
    lastMove = None
    if moveNum != 0:
        lastMove = re.search('^ \\(([BW]) (Pass|<(\d+),(\d+)>)\\)',
                             moveState)
        color,status = lastMove.group(1,2)
        if status != 'Pass':
            status = [int(s) for s in lastMove.group(3,4)]
        moveState = moveState[lastMove.end():]
        lastMove = {'color': color, 'status': status}

    toMove = re.search('^: ([BW]) to move$',moveState).group(1)
    whiteCaptives = int(re.search('^B: (\d+)$',lines[1]).group(1))
    blackCaptives = int(re.search('^W: (\d+)$',lines[2]).group(1))

    board = [['.'] * 19 for i in xrange(19)]
    coords = [zip([i]*19,range(19)) for i in xrange(19)]

    for l,c in zip(lines[4:23],coords):
        l = l[1:-1]
        for val,ind in zip(l,c):
            board[ind[0]][ind[1]] = val

    bIll = lines[-2][len("B illegal:"):].split(' ')
    print repr(bIll)
    wIll = lines[-1][len("W illegal:"):].split(' ')
    print repr(wIll)
    bIll = [(ord(s[0])-ord('a'),ord(s[1])-ord('a'))
            for s in bIll if s != '']
    wIll = [(ord(s[0])-ord('a'),ord(s[1])-ord('a'))
            for s in wIll if s != '']

    return {
        'moveNum': moveNum,
        'lastMove': lastMove,
        'toMove': toMove,
        'whiteCaptives': whiteCaptives,
        'blackCaptives': blackCaptives,
        'board': board,
        'blackIllegal': bIll,
        'whiteIllegal': wIll,
        'raw': '\n'.join(lines)
    }

def readBoard(f):
    line = f.readline().rstrip()
    lines = []
    while line == "" or line[0] != "@":
        print line
        lines.append(line)
        line = f.readline().rstrip()
    return parseBoard(lines)

def sendMove(f,black,x,y):
    passMove = (x is None or y is None)
    if not passMove:
        x,y = int(x),int(y)
    legal = not passMove and x >= 0 and x <= 18 and y >= 0 and y <= 18
    if legal:
        x = string.ascii_lowercase[x];
        y = string.ascii_lowercase[y];
    loc = '[]' if not legal else '[' + x + y + ']'
    f.write(('B' if black else 'W') + loc + ';')
    f.flush()

def spawnAI(playerId,isBlack,aiType):
    if aiType not in aicontrol.aiLoad:
        return
    print "SPAWNING {} FOR {}".format(aiType,playerId)
    with aiLock:
        ais.append((aiType,[playerId,isBlack],playerId,
                    aicontrol.aiLoad[aiType]))
        aiLock.notifyAll()

def makeGame(ai = None,aiTypes = ['py-random','py-random'],
             names=['Black','White']):
    print repr(aiTypes)
    gameId = uniqueUuid(games)
    with userLock:
        bId = uniqueUuid(gameForUser)
        wId = uniqueUuid(gameForUser)
        o = {}
        o['id'] = gameId
        o['players'] = (bId,wId)

        o['popen'] = Popen(['../sgf-parse/build/sgfparse','--'],stdout=PIPE,stdin=PIPE)
        p = o['popen']
        mkdir_p('gamerecords')
        o['startTime'] = datetime.now()
        o['aiTypes'] = aiTypes

        if ai is not None:
            aiInd = 0
            if ai == 0 or ai < 0:
                aiType = aiTypes[aiInd]
                if aiType in aicontrol.aisByName:
                    names[0] = [x for x,y in aicontrol.aiNames
                                    if y == aiType][0]
                aiInd += 1
            if ai == 1 or ai < 0:
                aiType = aiTypes[aiInd]
                if aiType in aicontrol.aisByName:
                    names[1] = [x for x,y in aicontrol.aiNames
                                    if y == aiType][0]
        o['names'] = names

        p.stdin.write(';OF[gamerecords/' + gameId + '.sgf]P[]' +
                    'RU[Chinese]KM[7.50]PB[' + names[0] + ']PW[' +
                    names[1] + ']DT[' + str(o['startTime'].date()) +
                    ']C[' + str(o['startTime']) +'];')
        p.stdin.flush()
        p.stdout.readline()

        o['board'] = readBoard(p.stdout);

        o['update'] = Condition()
        o['lastPlayed'] = time.time()
        o['watchdog'] = Condition()
        o['queuedfor'] = set()
        o['hosts'] = ['Human','Human']

        with o['update']:
            games[gameId] = o
            gameForUser[bId] = (gameId,0)
            gameForUser[wId] = (gameId,1)

            if ai is not None:
                o['ai'] = ai
                aiInd = 0
                if ai == 0 or ai < 0:
                    aiType = aiTypes[aiInd]
                    o['hosts'][0] = 'Queued'
                    o['queuedfor'].add(bId)
                    spawnAI(bId,True,aiType)
                    aiInd += 1
                if ai == 1 or ai < 0:
                    aiType = aiTypes[aiInd]
                    o['hosts'][1] = 'Queued'
                    o['queuedfor'].add(wId)
                    spawnAI(wId,False,aiType)
            o['board']['whost'] = o['hosts'][1]
            o['board']['bhost'] = o['hosts'][0]
            o['update'].notifyAll()

    def watchdog(cond,gameId,timeout=30):
        assert(gameId in games)
        game = games[gameId]
        with cond:
            game['active'] = True
            while len(game['queuedfor']) != 0 or (game['active'] and \
                    ('ai' not in game or game['ai'] >= 0 or \
                     time.time()-game['lastPlayed'] < timeout)):
                game['active'] = False
                cond.wait(timeout)
            with game['update']:
                print "Killing game %s" % gameId
                (b,w) = game['players']
                try:
                    if game['popen'].poll() is None:
                        if not game['popen'].stdin.closed:
                            game['popen'].stdin.write(')')
                            game['popen'].stdin.close()
                    if game['popen'].poll() is None:
                        game['popen'].terminate()
                    if game['popen'].poll() is None:
                        game['popen'].kill()
                except:
                    pass
            game['popen'].wait()

            with game['update']:
                with userLock:
                    if b in gameForUser:
                        del gameForUser[b]
                    if w in gameForUser:
                        del gameForUser[w]
                if 'done' not in game['board'] or \
                        not game['board']['done']:
                    game['board']['done'] = True
                    endGame(gameId,game)
                game['update'].notifyAll()

            print "Game %s killed by watchdog" % gameId

    watcherThread = Thread(target=watchdog,
                           args=[o['watchdog'],gameId])
    watcherThread.daemon = True
    watcherThread.start()

    return o

def gameStatus(k,v):
    done = ('done' in v['board'] and v['board']['done'])
    bname,wname = v['names']
    bname += " ({})".format(v['hosts'][0])
    wname += " ({})".format(v['hosts'][1])

    state = 'Interrupted'
    if not done:
        if len(v['queuedfor']) != 0:
            state = 'Queued'
        else:
            state = 'Active'
    elif 'scores' in v['board']:
        bscore,wscore = v['board']['scores']
        diff = bscore-wscore
        win='B' if diff > 0 else 'W'
        state = '{}+{}'.format(win,abs(diff)/2.0)

    return (k,bname,wname,state,v['startTime'])

@app.route('/view')
def viewGames():
    status = [(v,gameStatus(k,v)) for k,v in games.iteritems()]
    workerStats = []
    with workerLock:
        for k,v in workers.iteritems():
            workerStats.append((v['id'],v['name'],v['tasks'],
                                v['avail'],v['maxAvail'],
                                v['running'],v['version']))
    return render_template('view.html',
            games=[s for g,s in
                    sorted(status,key=lambda g: g[0]['startTime'])],
            workers=workerStats,tourney=runningTourney,
            version=serverVersion)

@app.route('/games')
def listGames():
    status = [(v,gameStatus(k,v)) for k,v in games.iteritems()]
    workerStats = []
    with workerLock:
        for k,v in workers.iteritems():
            workerStats.append((v['id'],v['name'],v['tasks'],
                                v['avail'],v['maxAvail'],
                                v['running'],v['version']))
    return render_template('games.html',
            games=[s for g,s in
                    sorted(status,key=lambda g: g[0]['startTime'])],
            workers=workerStats,tourney=runningTourney,
            version=serverVersion)

@app.route('/go')
def gameSetup():
    return render_template('setup.html',
            ais=aicontrol.aiNames)

@app.route('/go/board')
def goban(name=None):
    playerId = request.args.get('player',None)
    gameId = request.args.get('game',None)
    if (gameId is None or gameId not in games) and \
            (playerId is None or playerId not in gameForUser):
        return redirect('..' + url_for('startGame'))

    idx = None
    if playerId is not None:
        gameId,idx = gameForUser[playerId]

    game = games[gameId]
    bname = game['names'][0];
    wname = game['names'][1];

    if playerId is not None and \
            ('ai' not in game or (game['ai'] >= 0 and \
             game['ai'] != idx)):
        player,other = "B","W"
        otherInd = 1-idx
        if idx == 1:
            player,other = other,player

        otherurl = '..' + url_for('goban')+"?player="+str(game['players'][otherInd])
        spectateurl = '..' + url_for('goban')+"?game="+gameId

        return render_template('go.html', player=player,other=other,
                            otherurl=otherurl,spectateurl=spectateurl,
                            key=playerId,name=name,bname=bname,
                            wname=wname,
                            startTime=str(game['startTime']))
    elif idx is not None and 'ai' in game and (game['ai'] < 0 or game['ai'] == idx):
        return redirect('..' + url_for('goban')+"?game=" + gameId)
    else:
        player = "Spectator"
        return render_template('go.html', player=player,
                               key=gameId,name=name,bname=bname,
                               wname=wname,
                               startTime=str(game['startTime']))

@app.route('/go/start')
def startGame(name=None):
    ai = request.values.get('ai',None)
    names = [request.values.get('bname','Black'),
             request.values.get('wname','White')]
    aiTypes = [request.values.get('ai1','py-random'),
               request.values.get('ai2','py-random')]
    color = request.values.get('color',None)
    if str(ai) == 'no':
        ai = None
    if ai is not None:
        if int(ai) >= 2:
            ai = -1
        else:
            if color is not None:
                color = int(color)
                ai = 1 - color
            ai = int(ai)
    if ai is not None:
        color = 1 - ai if ai >= 0 else 0
    elif color is None:
        color = 0
    color=int(color)

    game = makeGame(ai,aiTypes,names)
    bid = game['players'][color]
    return redirect('..' + url_for('goban')+"?player=" + bid)

@app.route('/api/offerwork', methods=['POST'])
def offerwork():
    global ais
    print "AIs: {}".format(ais)

    data = json.loads(request.data)
    workerId = data.get('key','')
    tasks = data.get('tasks',[])
    host = data.get('host',workerId)
    load = data.get('avail',[0,0])
    load = WorkerLoad(load[0],load[1])
    maxLoad = data.get('maxAvail',[0,0])
    maxLoad = WorkerLoad(maxLoad[0],maxLoad[1])
    running = data.get('running',[])
    version = data.get('version','')

    refreshWorker(workerId,{
        'id': workerId,
        'name': host,
        'tasks':tasks,
        'avail':load,
        'maxAvail':maxLoad,
        'running':running,
        'version':version
    })

    # print
    # print "{} offers {}: maxLoad is {}, running {}" \
    #         .format(workerId,tasks,load,running)
    with queueLock:
        if workerId in tryingToStart:
            starting = tryingToStart[workerId]
            # print "Running: {}".format(running)
            for tid,_ in running:
                # print "Tid: {}".format(tid)
                if tid in starting:
                    g = games[gameForUser[tid][0]]
                    with g['update']:
                        if g['players'][0] == tid:
                            g['hosts'][0] = host
                            g['board']['bhost'] = host
                        else:
                            g['hosts'][1] = host
                            g['board']['whost'] = host
                        g['queuedfor'].discard(tid)
                        g['update'].notifyAll()
                    del starting[tid]
            if len(starting) != 0:
                with aiLock:
                    ais = [t for tid,t in starting.iteritems()
                            if tid in gameForUser] + ais
                    aiLock.notifyAll()
            del tryingToStart[workerId]

    if version != serverVersion:
        print "Server: {}".format(serverVersion)
        print "Worker: {}".format(version)
        return json.jsonify({'newTasks':[],'update':True})

    newTasks = []

    with aiLock:
        if len(ais) == 0:
            aiLock.wait(3)
        grabbedTasks = set()
        for t,params,pid,tload in ais:
            if t not in tasks:
                continue
            else:
                if tload.fits(load):
                    print "Starting {} for {} on {}" \
                            .format(t,pid,workerId)
                    load -= tload
                    newTasks.append((t,params,pid))
                    grabbedTasks.add(pid)
                    if workerId not in tryingToStart:
                        tryingToStart[workerId] = {}
                    tryingToStart[workerId][pid] = \
                        (t,params,pid,tload)
                else:
                    print "Can't fit {}".format(t)
                    break
        ais = [a for a in ais if a[2] not in grabbedTasks]
        print "New ais: {}".format(ais)
        aiLock.notifyAll()

        return json.jsonify({'newTasks':newTasks,'update':False})

@app.route('/api/tourney', methods=['GET'])
def setTourney():
    global runningTourney
    print "Running tourney? {}".format(repr(runningTourney))
    runningTourney = (request.args.get('tourney',
                        '1' if runningTourney else '0') == '1')
    print "New Running tourney? {}".format(repr(runningTourney))

    return redirect('.' + url_for('listGames'))

@app.route('/api/board', methods=['POST'])
def board():
    playerId = request.form.get('key',None)
    if playerId is None or \
            (playerId not in gameForUser and playerId not in games):
        return make_response(("",404,{}))

    gameId = playerId
    if playerId in gameForUser:
        gameId,idx = gameForUser[playerId]

    game = games[gameId]

    isai = request.form.get('isai',False)
    if not isai:
        with game['watchdog']:
            game['active'] = True
            game['watchdog'].notify()

    return json.jsonify(game['board'])

@app.route('/api/boardchange',methods=['POST'])
def boardchange():
    playerId = request.form.get('key',None)
    moveNum = request.form.get('move',-1)
    if playerId is None or \
            (playerId not in gameForUser and playerId not in games):
        return make_response(("",404,{}))

    gameId = playerId
    if playerId in gameForUser:
        gameId,idx = gameForUser[playerId]

    game = games[gameId]
    isai = request.form.get('isai',False)
    if not isai:
        with game['watchdog']:
            game['active'] = True
            game['watchdog'].notify()

    with game['update']:
        if moveNum >= game['board']['moveNum']:
            game['update'].wait(2.5)
        return json.jsonify(game['board'])

@app.route('/api/play', methods=['POST'])
def play():
    playerId = request.form.get('key',None)
    x = request.form.get('x',None)
    y = request.form.get('y',None)
    isai = request.form.get('isai',False)

    try:
        x = int(x)
        y = int(y)
    except:
        x = None
        y = 0
    if playerId is None or playerId not in gameForUser:
        return make_response(("",404,{}))

    gameId,idx = gameForUser[playerId]
    game = games[gameId]

    with game['watchdog']:
        game['active'] = True
        game['watchdog'].notify()
    game['lastPlayed'] = time.time()

    if 'done' in game['board'] and game['board']['done']:
        return make_response(("Game Over",410))

    p = game['popen']
    resp = None
    with game['update']:
        print "Playing move {}({},{})" \
            .format('B' if idx == 0 else 'W',x,y)
        sendMove(p.stdin,(idx == 0),x,y)
        resp = p.stdout.readline().rstrip()

        if resp in ['*','=']:
            game['board'] = readBoard(p.stdout);
            if len(game['queuedfor']) != 0:
                game['board']['bqueued'] = game['players'][0] in \
                                           game['queuedfor']
                game['board']['wqueued'] = game['players'][1] in \
                                           game['queuedfor']
            else:
                game['board']['bqueued'],game['board']['wqueued'] = \
                    False,False
            game['board']['whost'] = game['hosts'][1]
            game['board']['bhost'] = game['hosts'][0]

            if resp == '=':
                b,w = map(int,p.stdout.readline().rstrip().split(','))
                game['board']['done'] = True
                game['board']['scores'] = [b,w]
                endGame(gameId,game)
                print "WAITING FOR GAME TO CLOSE"
                try:
                    if p.poll() is None:
                        if not p.stdin.closed:
                            p.stdin.write(')')
                            p.stdin.close()
                    if p.poll() is None:
                        p.terminate()
                    if p.poll() is None:
                        p.kill()
                except:
                    pass
                p.wait()
                print "DELETING GAME"

                bid,wid = game['players']
                if bid in gameForUser:
                    del gameForUser[bid]
                if wid in gameForUser:
                    del gameForUser[wid]
            game['update'].notifyAll()
            return json.jsonify(game['board'])

    return make_response((resp,500 if resp == '?' else 400,{}))

def gameFits(bai,wai):
    if bai not in aicontrol.aiLoad or wai not in aicontrol.aiLoad:
        return False
    loads = [aicontrol.aiLoad[bai],aicontrol.aiLoad[wai]]
    totalLoad = loads[0] + loads[1]
    fitFound = [False,False]
    with workerLock:
        bwid = None
        for wid,dat in workers.iteritems():
            if totalLoad.fits(dat['maxAvail']):
                return True
            if loads[0].fits(dat['maxAvail']):
                fitFound[0] = True
            if loads[1].fits(dat['maxAvail']):
                fitFound[1] = True
            if all(fitFound):
                return True
    return False

def allAvailableAis():
    with workerLock:
        return [x for _,w in workers.iteritems()
                     for x in w['tasks']]

def runTourney():
    global currTourneyGame
    global nextTourneyGame
    global ais
    while True:
        # print "Trying to run tourney"
        if runningTourney and currTourneyGame is not None:
            # print "Waiting for current game to dequeue"
            with aiLock:
                while len(currTourneyGame['queuedfor']) != 0 and \
                        gameFits(*currTourneyGame['aiTypes']):
                    aiLock.notifyAll()
                    aiLock.wait(1)
                if len(currTourneyGame['queuedfor']) != 0:
                    currTourneyGame['done'] = True
                    badnames = currTourneyGame['aiTypes']
                    ais = [a for a in ais if a[0] not in badnames]
            currTourneyGame = None

        if runningTourney and nextTourneyGame is None:
            # print "Generating next game"
            avail = allAvailableAis()
            found = False
            b,w = None,None
            while not found:
                b = random.choice(avail)
                w = random.choice(avail)
                found = gameFits(b,w)
            nextTourneyGame = (b,w)
            # print "Found game: {}".format(nextTourneyGame)

        if runningTourney:
            # print "Starting game"
            b,w = nextTourneyGame
            r = app.test_client() \
                .get('/go/start?color=0&ai=2&ai1=' + b + '&ai2=' + w)
            # print "Got response: {}".format(r)
            key = re.search('player=(.*)$',r.location).group(1)
            if key in gameForUser:
                key = gameForUser[key][0]
            currTourneyGame = games[key]
            nextTourneyGame = None
            # print "Game started: {}".format(key)

        with aiLock:
            aiLock.wait(3)

if __name__ == '__main__':
    #app.debug = True
    #app.use_debugger = True
    t = Thread(target=runTourney)
    t.daemon = True

    def watcher():
        global t
        while True:
            if not t.is_alive():
                t = Thread(target=runTourney)
                t.daemon = True
                t.start()

            time.sleep(5)

    watch = Thread(target=watcher)
    watch.daemon = True
    watch.start()

    app.run(host='localhost',threaded=True,port=51337) #debug=True, use_debugger=True)

