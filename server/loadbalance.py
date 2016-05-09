from threading import Condition,Thread

class WorkerLoad:
    cpuLoad = 0 # execution context count
    memLoad = 0 # total memory in gigabytes

    def __init__(self,cpu = 0,mem = 0):
        self.cpuLoad = cpu
        self.memLoad = mem

    def __add__(self,other):
        return WorkerLoad(self.cpuLoad+other.cpuLoad,
                          self.memLoad+other.memLoad)

    def __sub__(self,other):
        return WorkerLoad(self.cpuLoad-other.cpuLoad,
                          self.memLoad-other.memLoad)

    def __str__(self):
        return "{} cpu, {} mem".format(self.cpuLoad,self.memLoad)

    def fits(self,maxVal):
        return self.cpuLoad <= maxVal.cpuLoad and \
               self.memLoad <= maxVal.memLoad

    @classmethod
    def maxLoad(cls):
        import multiprocessing
        from psutil import virtual_memory

        return cls(multiprocessing.cpu_count()-2,
                   virtual_memory().total/1e9-2)

