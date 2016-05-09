import requests,random,time,sys
from subprocess import Popen,PIPE
import traceback
from loadbalance import WorkerLoad
import socket

mypath = sys.argv[0]
print mypath
if '/' in mypath:
    mypath = mypath[:mypath.rfind('/')]
else:
    mypath = '.'

jpdoylecert=mypath + "/../jpdoyle-cert.pem"
def enableCoreDump():
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (-1, -1))

def generalAI(playerId,isBlack,ai,addr='localhost:51337'):
    ai.setColor(isBlack)
    print "{} STARTING".format(ai.name)
    board = None
    while board is None or 'done' not in board or not board['done']:
        try:
            while board is None:
                print "%s getting first board" % \
                    ('B' if isBlack else 'W')
                print "GETTING NEW BOARD"
                r = requests.post('https://' + addr + '/api/board',
                                    data={'key':playerId,'isai':True},
                                    verify=jpdoylecert)
                r.raise_for_status()
                board = r.json()
                ai.updateBoard(board)

            if (board['toMove'] == 'B') == isBlack:
                nextMove = ai.getNextMove()
                x,y = nextMove if nextMove is not None \
                               else (None,None)
                isPass = any(v is None for v in [nextMove,x,y])

                print "TRYING (" + str(x) + "," + str(y) + ")"
                data = {'key':playerId,'isai':True}
                if not isPass:
                    data['x'] = x
                    data['y'] = y
                print "PLAYING"
                r = requests.post('https://' + addr + '/api/play',
                        data=data,verify=jpdoylecert)

                print str(r)
                if r.status_code == 400:
                    print "BAD MOVE"
                    ai.badMove()
                    continue
                r.raise_for_status()
                board = r.json()
                ai.updateBoard(board)
                print "PLAYED (" + str(x) + "," + str(y) + ")"

            mycolor = 'B' if isBlack else 'W'
            moveNum = board['moveNum']
            while board['moveNum'] == moveNum and \
                  mycolor != board['toMove'] and \
                  (not ('done' in board) or not board['done']):
                print "GETTING NEW BOARD"
                r = requests.post(
                        'https://' + addr + '/api/boardchange',
                        data={'key':playerId,'isai':True,
                                'move':moveNum},verify=jpdoylecert)
                r.raise_for_status()
                board = r.json()
                ai.updateBoard(board)

        except Exception as e:
            print "ERROR: " + str(e)
            print "{} says: '{}'".format(ai.name,ai.getError())
            traceback.print_exc()
            break

        time.sleep(0.1)
    print "{} SHUTTING DOWN".format(ai.name)
    ai.shutDown()
    print "{} DONE".format(ai.name)

def makeGeneralAI(constructor):
    def ai(playerId,isBlack,addr='localhost:51337'):
        generalAI(playerId,isBlack,constructor(),addr)
    return ai

class RandomMoveAI:
    name = "RANDOMBOT"
    isBlack = True
    board = None
    legalMoves = []

    def setColor(self,isBlack):
        self.isBlack = isBlack

    def updateBoard(self,board):
        if board is not None:
            self.board = board
            illegalMoves = board['blackIllegal' if self.isBlack
                                 else 'whiteIllegal']
            illegalMoves = set(tuple(c) for c in illegalMoves)
            self.legalMoves = [(x,y)
                        for y,row in enumerate(board['board'])
                        for x,c in enumerate(row)
                        if c == '.' and (x,y) not in illegalMoves]

    def getNextMove(self):
        if self.legalMoves != []:
            return random.choice(self.legalMoves)
        else:
            return None

    def shutDown(self):
        pass

    def badMove(self):
        pass

class CppAI:

    name = "CppBOT"
    isBlack = True
    board = None

    def __init__(self,params=['--randombot']):
        self.name += ':' + str(params)
        self.popen = Popen(['../sgf-parse/build/sgfparse'] + params,
                           stdin=PIPE,stdout=PIPE,bufsize=4096,
                           preexec_fn=enableCoreDump)

    def setColor(self,isBlack):
        print "SENDING COLOR TO {}".format(self.name)
        self.isBlack = isBlack
        p = self.popen
        col = 'B' if isBlack else 'W'
        p.stdin.write(col + '\n')
        p.stdin.flush()
        reply = p.stdout.readline().rstrip()
        print "GOT '{}' FROM {}".format(reply,self.name)
        assert(reply == col)

    def updateBoard(self,board):
        if self.board is not None and board is not None:
            if board['raw']  == self.board['raw']:
                return
        self.board = board
        print "SENDING BOARD TO {}".format(self.name)
        p = self.popen
        if board is not None:
            if 'done' in board and board['done']:
                self.shutDown()
                return
            p.stdin.write('*\n')
            p.stdin.write(board['raw'] + '\n')
            p.stdin.flush()
            reply = p.stdout.readline().rstrip()
            print "GOT '{}' FROM {}".format(reply,self.name)
            assert(reply == '+')


            self.board = board
            illegalMoves = board['blackIllegal' if self.isBlack
                                 else 'whiteIllegal']
            illegalMoves = set(tuple(c) for c in illegalMoves)
            self.legalMoves = [(x,y)
                        for y,row in enumerate(board['board'])
                        for x,c in enumerate(row)
                        if c == '.' and (x,y) not in illegalMoves]

    def getNextMove(self):
        print "GETTING MOVE FROM {}".format(self.name)
        p = self.popen
        time.sleep(0.25)
        p.stdin.write('?\n')
        p.stdin.flush()
        move = p.stdout.readline().rstrip()
        print "GOT '{}' FROM {}".format(move,self.name)
        try:
            if ' ' in move:
                x,y = [int(w) for w in move.split(' ')]
                return (x,y)
        except:
            return None

    def getError(self):
        p = self.popen
        if not p.stdin.closed:
            print "{}: Closing stdin".format(self.name)
            p.stdin.close()

        ret = "Not exitted"

        if p.poll() is not None:
            ret = "Error {}".format(p.poll()) + ret

        if not p.stdout.closed:
            print "{}: Closing stdout".format(self.name)
            p.stdout.close()


        return ret

    def shutDown(self):
        p = self.popen
        self.getError()
        time.sleep(1)
        if p.poll() is None:
            p.terminate()
        time.sleep(1)
        if p.poll() is None:
            p.kill()

        return p.wait()

    def badMove(self):
        pass

def randomMoveAI():
    return makeGeneralAI(RandomMoveAI)

def cppAI():
    return makeGeneralAI(CppAI)
def oneMoveAI():
    return makeGeneralAI(lambda: CppAI(['--onemovebot']))
def monteCarloAI(numThreads,nn=None):
    params = ['--montecarlo']
    if numThreads != 1 or nn is not None:
        params.append(str(int(numThreads)))
    if nn is not None:
        params.append(str(int(nn)))
        if "cmu.edu" in socket.gethostname():
            params.append("afs")
    return makeGeneralAI(lambda: CppAI(params))

def nnAI(numLayers):
    params = ['--nn']
    if "cmu.edu" in socket.gethostname():
        params = ['--nnafs']
    if numLayers != 1:
        params.append(str(int(numLayers)))
    return makeGeneralAI(lambda: CppAI(params))

aiNames = [
    ('RandomBot','cpp-random'),
    ('One-move search','cpp-onemove'),
    ('Neural network (3 inner layers)', 'cpp-nn3'),
    ('Neural network (4 inner layers)', 'cpp-nn4'),
    ('Neural network (5 inner layers)', 'cpp-nn5'),
    ('Neural network (6 inner layers)', 'cpp-nn6'),
    ('Deep Neural network (11 inner layers)', 'cpp-deep'),
    ('AP-MCTS (MCTS4-NN3)','nn3-mcts4'),
    ('AP-MCTS (MCTS4-NN6)','nn6-mcts4'),
    ('AP-MCTS (MCTS4-Deep)','deep-mcts4'),
    ('AP-MCTS (MCTS6-NN3)','nn3-mcts6'),
    ('AP-MCTS (MCTS6-NN6)','nn6-mcts6'),
    ('AP-MCTS (MCTS6-Deep)','deep-mcts6'),
    ('Simple MCTS (1 thread)','cpp-mcts'),
    ('Simple MCTS (2 thread)','cpp-mcts2'),
    ('Simple MCTS (4 thread)','cpp-mcts4'),
    ('Simple MCTS (5 thread)','cpp-mcts5'),
    ('Simple MCTS (6 thread)','cpp-mcts6'),
    ('Simple MCTS (8 thread)','cpp-mcts8')
]

aisByName = {
    'cpp-random': cppAI(),
    'cpp-onemove': oneMoveAI(),
    'cpp-nn3': nnAI(3),
    'cpp-nn4': nnAI(4),
    'cpp-nn5': nnAI(5),
    'cpp-nn6': nnAI(6),
    'cpp-deep': nnAI(0),
    'nn3-mcts4':  monteCarloAI(4,3),
    'nn6-mcts4':  monteCarloAI(4,6),
    'deep-mcts4': monteCarloAI(4,0),
    'nn3-mcts6':  monteCarloAI(6,3),
    'nn6-mcts6':  monteCarloAI(6,6),
    'deep-mcts6': monteCarloAI(6,0),
    'cpp-mcts':  monteCarloAI(1),
    'cpp-mcts2':  monteCarloAI(2),
    'cpp-mcts4':  monteCarloAI(4),
    'cpp-mcts5':  monteCarloAI(5),
    'cpp-mcts6':  monteCarloAI(6),
    'cpp-mcts8':  monteCarloAI(8),
}

aiLoad = {
    'cpp-random': WorkerLoad(0.1,0),
    'cpp-onemove': WorkerLoad(0.5,0),
    'cpp-nn3': WorkerLoad(1.5,1.5),
    'cpp-nn4': WorkerLoad(1.75,1.75),
    'cpp-nn5': WorkerLoad(2,2),
    'cpp-nn6': WorkerLoad(2.25,2),
    'cpp-deep': WorkerLoad(4.5,4.25),
    'nn3-mcts4':   WorkerLoad(5,1.5),
    'nn6-mcts4':   WorkerLoad(6,2.5),
    'deep-mcts4':  WorkerLoad(6.5,4.5),
    'nn3-mcts6':   WorkerLoad(7,1.5),
    'nn6-mcts6':   WorkerLoad(8.25,2.5),
    'deep-mcts6':  WorkerLoad(10.5,4.75),
    'cpp-mcts':   WorkerLoad(1,0.5),
    'cpp-mcts2':  WorkerLoad(2,0.5),
    'cpp-mcts4':  WorkerLoad(4,0.5),
    'cpp-mcts5':  WorkerLoad(5,0.5),
    'cpp-mcts6':  WorkerLoad(6,0.5),
    'cpp-mcts8':  WorkerLoad(8,0.5)
}

