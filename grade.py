# change TYPE as your Intention
import os
TESTDIR = "/home/wynn/Desktop/codes/sfsecurity/se-fault-tolerant-template-master/build/out/bin/test/"
NUM = 1
TYPE = "unit--log"
for f in os.listdir(TESTDIR):
    if os.access(TESTDIR+f, os.X_OK) and f.startswith(TYPE):
        print("*******************************************************************")
        print("{} ".format(NUM) + f)
        os.system(TESTDIR+f)
        print("*******************************************************************")
        print("\n")
        NUM += 1
