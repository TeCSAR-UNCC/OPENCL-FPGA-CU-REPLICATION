import os
import commands
import csv
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
cwd = os.getcwd()
#################################### PARAMETERS TO CHANGE #######################################
# Line 1: Provide the path for the directory
# Line 2: Provide the entire LDCLFLAG command line for compute unit replication
# Line 3: Provide the argument for running the host code if there is any
#################################################################################################
PATH = cwd #+ "/affine"
Makefile_CU_line = "krnl_affine_LDCLFLAGS = --nk affine_kernel:"
Execution = "check_ARGS = ./data/CT-MONO2-16-brain.raw"
#################################### Compute Unit replication  ################################
compute_units = []
work_group = [1,2,4,5,6,7,8,9,10]
CU_list = []
Total_size = (512)
########### Change the current working directory to the path ###################################
if PATH != cwd:
    os.chdir(PATH)
####################################### Compute replication ######################################
for k in range(1,10):
########################################## Edit MakeFile ######################################### 
    line_no = 0
    readfile = open("Makefile","r+")
    readline = readfile.readlines()
    readfile.close()
    temp = readline
    for line in readline:
        line_no = line_no + 1
        sin_line = line.split('\n')
        for i in sin_line:
            m_cu = i.split(' ')
            for j in m_cu:
                if j =='--nk':
                    temp[line_no-1] = Makefile_CU_line + str(k) + "\n"
                #print j
                if j =="check_ARGS":
                    temp[line_no-1] = Execution +" "+ str(k) + "\n" 
    temp = "".join(temp)
    f = open("Makefile","w+")
    f.write(temp)
    f.close()
    Kernel_Exec_Time = "Kernel Execution"
    #########################  Run Hardware Emulation ##############################################
    os.system("make clean")
    os.system("make check TARGETS=hw_emu PROFILE=system REPORT=estimate DEVICES=$AWS_PLATFORM all")
    Host_to_global_mem = "Data Transfer: Host and Global Memory"
    CU_Exec_Time = "Compute Unit Utilization (includes estimated device times)"
    #########################  CSV File Reading ######################################################
    H_to_G = 0
    H_to_Glist = []
    time_list = []
    used_cu = 0
    sum = 0
    csvfile = open("sdaccel_profile_summary.csv",'r') 
    read_file = csv.reader(csvfile, delimiter=',', quotechar='|')
    for row in read_file:
        for field in row:
            if field == Host_to_global_mem:
                header = read_file.next()
                Bandwidth_h_g = header.index("Average Bandwidth Utilization (%)") 
                Transfer_h_g  = header.index("Transfer Rate (MB/s)")
                while header != []:
                    H_to_Glist.append([header[Bandwidth_h_g],header[Transfer_h_g]])
                    header = read_file.next()
            if field == CU_Exec_Time:
                header = read_file.next()
                cu_name = header.index("Compute Unit")
                Total_Time = header.index("Total Time (ms)")
                header = read_file.next()
                while header != []:
                    sum = sum + float(header[Total_Time])
                    used_cu = used_cu + 1;
                    #time_list.append(header[Total_Time])
                    header = read_file.next()
    avg_time = sum/used_cu;
    time_list.append(avg_time)
    csvfile.close()
    CU_list.append(float(time_list[-1]))
    print time_list
    compute_units.append(k)
    print CU_list
########################################Checks the resource Utilization ################################
    area = "Area Information"
    resource_utilization = open("system_estimate.xtxt",'r')
    readline = resource_utilization.readlines()
    resource_utilization.close()
    temp = readline;
    line_no = 0;
    count = 0
    FlipFlop = 0
    lut = 0
    dsp = 0
    bram = 0
    for lines in readline:
        sin_line = lines.split('\n')
        line_no = line_no + 1
        for i in sin_line:
            if i == area:
                line_no = line_no + 2
                resource_values = temp[line_no].split(' ')
                while True:
                    for s in resource_values:
                        if s == '':
                            count = count+1
                    while count!= 0:
                        resource_values.remove('')
                        count= count - 1
                    print len(resource_values)
                    if len(resource_values) != 7: break
                    FlipFlop += int(resource_values[3])
                    lut += int(resource_values[4])
                    dsp += int(resource_values[5])
                    bram += int(resource_values[6])
                    line_no = line_no + 1 
                    resource_values = temp[line_no].split(' ')
                print FlipFlop
                ff_percent = (FlipFlop/2331914)
                lut_percent = (lut/1157312)
                dsp_percent = (dsp/6840)
                bram_percent = (bram/2134)   
                if (ff_percent == 1) or (lut_percent==1) or (dsp_percent==1) or (bram_percent==1):
                    break
################################### Checks the difference between the execution time value ##################
    if len(CU_list) > 2:
        delta = float(CU_list[-2]) - float(CU_list[-1])
        if delta < 0.5:
            print delta
            break
################################### PLOT THE  TIME #############################
plt.bar(compute_units, CU_list)
plt.xlabel('Compute Unit')
plt.ylabel('Execution Time')
plt.savefig('Hardware_emulation.png')
