import os
import commands
import csv
import matplotlib
matplotlib.use('agg')
import matplotlib.pyplot as plt
HW_EMU = 1
SW_EMU = 0
cwd = os.getcwd()
#################################### PARAMETERS TO CHANGE #######################################
PATH = cwd 
Makefile_CU_line = "krnl_tiny_LDCLFLAGS = --nk tinyEncryption:"
Execution = "check_ARGS = data/image1.bmp data/image2.bmp ./xclbin/krnl_tiny.$(1).$(call device2sandsa,$(2)).xclbin "
Kernel_file = PATH + "src/krnl_tinyEncryption.cl"
#kernel_file = "affine/src/krnl_affine.cl"
compute_units = []
work_group = [1,2,4,5,6,7,8,9,10]
CU_list = []
Total_size = (8192)
###########Working files####################
if PATH != cwd:
    os.chdir(PATH)
#################################################################             Compute Unit replication              #######################################################################
for k in range(1,10):
    ##################  Edit Kernel file  ###############################
    line_no = 0
    readfile = open("src/krnl_tinyEncryption.cl","r+")
    readline = readfile.readlines()
    readfile.close()
    temp = readline
    for line in readline:
        line_no = line_no + 1
        sin_line = line.split('\n')
        for i in sin_line:
            m_cu = i.split(' ')
            for j in m_cu:
                if j =='buffer':
                    temp[line_no-1] = "#define buffer " + str(Total_size/k) + "\n"
                #print j
                
    temp = "".join(temp)
    f = open("src/krnl_tinyEncryption.cl","w+")
    f.write(temp)
    f.close()
    ################## Edit MakeFile ################################# 
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

    #########################  Run the software Emulation ##########################################
    if SW_EMU:
        os.system("make clean")
        os.system("make check TARGETS=sw_emu PROFILE=system REPORT=estimate DEVICES=$AWS_PLATFORM all")    
        Host_to_global_mem = "Data Transfer: Host and Global Memory"
        CU_Exec_Time = "Compute Unit Utilization"
    #########################  Run Hardware Emulation ##############################################
    if HW_EMU:
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
    #print H_to_Glist
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
if SW_EMU:
    plt.savefig('Software_emulation.png')
if HW_EMU:
    plt.savefig('Hardware_emulation.png')
