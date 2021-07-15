import os
import subprocess
import sys

class Tester:

    testFileDirectory = "./test"

    testList = [
        "for", 
        "gcd",
        "test1",
        "test2",
        "test3",
    ]

    procname = "./hoc"

    def runTest(self, testName):
        filename = os.path.join(self.testFileDirectory, "{}.test".format(testName))
        procArgs = self.procname + " " + filename
        try:
            p = subprocess.Popen(procArgs, shell=True, stdout=subprocess.PIPE)
            stdout_data = p.communicate()[0]
        except Exception as e:
            print("Call of '{}' failed: {}".format(procArgs, e))
            return False
        
        resname = os.path.join(self.testFileDirectory, "{}.res".format(testName))
        res = open(resname, "rb")
        res_data = res.read()
        if (res_data == stdout_data):
            print("{} test passed".format(testName))
            return True
        else:
            print("{} test failed \ngot:\n{}\nexpecting:\n{}\n".format(testName, stdout_data.decode("utf-8"), res_data.decode("utf-8")))
            return False
        
        return True
    
    def run(self):
        res = True
        for testName in self.testList:
            res = self.runTest(testName) and res

        if res:
            print("All test passed!!!!")


t = Tester()
t.run()