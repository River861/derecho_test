from threading import Thread
import configparser

import subprocess
import platform


class CmdProcess(Thread):

    def __init__(self, cmd: str):
        super().__init__()
        self.__cmd = cmd
        self.__result = None

    def run(self):
        p = subprocess.Popen(self.__cmd, shell=True, stdout=subprocess.PIPE)
        return p.wait()
        # self.__result = p.communicate(input=None)

    def get_result(self):
        return self.__result


if __name__ == '__main__':
    # 由于Derecho中Conf类是个单例，导致不能直接使用C++多线程
    # 因此改为通过python脚本来fork出多个进程来跑(os.system)

    conf = configparser.ConfigParser()
    conf.read("derecho.cfg")
    local_id = conf.getint("DERECHO", "local_id")
    gms_port =  conf.getint("DERECHO", "gms_port")
    state_transfer_port = conf.getint("DERECHO", "state_transfer_port")
    sst_port = conf.getint("DERECHO", "sst_port")
    rdmc_port = conf.getint("DERECHO", "rdmc_port")
    external_port = conf.getint("DERECHO", "external_port")

    cmd_process = {
        i : CmdProcess(f"taskset -c {i*2} ./main "
                       f"  --DERECHO/local_id={local_id*8+i}"
                       f"  --DERECHO/gms_port={gms_port+i*20}"
                       f"  --DERECHO/state_transfer_port={state_transfer_port+i*20}"
                       f"  --DERECHO/sst_port={sst_port+i*20}"
                       f"  --DERECHO/rdmc_port={rdmc_port+i*20}"
                       f"  --DERECHO/external_port={external_port+i*20}")
        for i in range(8)
    }
    for p in cmd_process.values():  # 并发执行
        p.start()
    for p in cmd_process.values():
        p.join()
    for i, p in cmd_process.items():
        print(f"Thread {i} exit with {p.get_result()}.")
