import uuid,requests,time,json
from loadbalance import WorkerLoad
import multiprocessing,socket
import traceback
import os,sys

mypath = sys.argv[0]
print mypath
if '/' in mypath:
    mypath = mypath[:mypath.rfind('/')]
else:
    mypath = '.'

jpdoylecert=mypath + "/../jpdoyle-cert.pem"

os.nice(20)

# taskDict should map name to (taskFn,load)
def startWorker(taskDict,masterAddr):
    workerId = str(uuid.uuid4())
    host = socket.gethostname()
    tasks = []
    maxLoad = WorkerLoad.maxLoad()
    currentLoad = WorkerLoad()
    requestAddr = 'https://' + masterAddr + "/api/offerwork"
    print "Resources: {}".format(str(maxLoad))
    while True:
        try:
            avail = maxLoad - currentLoad;
            print "My tasks: {}".format(taskDict.keys())
            data = {'key':workerId,
                    'host':host,
                    'tasks':taskDict.keys(),
                    'maxAvail':[maxLoad.cpuLoad,maxLoad.memLoad],
                    'avail':[avail.cpuLoad,avail.memLoad],
                    'running':[(t[0],t[1]) for t in tasks]}
            print "My data: {}".format(data)
            r = requests.post(requestAddr,
                            data=json.dumps(data), timeout=5,verify=jpdoylecert)

            r.raise_for_status()
            task = r.json()
            print str(task)
            for t,params,taskId in task['newTasks']:
                print "New work offer: {},{},{}".format(t,params,taskId)
                if t in taskDict:
                    params = params + [masterAddr]
                    fn,load = taskDict[t]
                    if load.fits(avail):
                        p = multiprocessing.Process(target=fn,
                                                    args=params)
                        p.start()
                        tasks.append((taskId,t,params,p,load))
                        currentLoad += load
                        avail = maxLoad - currentLoad

            remainingIds = set()
            for taskId,_,_,p,load in tasks:
                if p.is_alive():
                    remainingIds.add(taskId)
                else:
                    p.join()
                    currentLoad -= load
                    time.sleep(3)

            tasks = [x for x in tasks if x[0] in remainingIds]

            print "Tasks remaining: {}".format(tasks)
            time.sleep(1)
        except Exception as e:
            print "ERROR: " + str(e)
            traceback.print_exc()
            time.sleep(5)

if __name__ == '__main__':
    import aicontrol
    import sys
    masterAddr = 'localhost:51337'
    if len(sys.argv) >= 2:
        masterAddr = sys.argv[1]
    tasks = { name:(fn,aicontrol.aiLoad[name])
                for name,fn in aicontrol.aisByName.iteritems()
                if aicontrol.aiLoad[name].fits(WorkerLoad.maxLoad()) }
    print "I have: {}".format(tasks)
    startWorker(tasks,masterAddr)

