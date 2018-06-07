#!/usr/bin/python


#get the information
#make the data structure
#analyze data structure

import csv
import sys

class blockInfo:
    offsets = -1
    inode = -1
    indirection = -1

class inodeInfo:
    isValid = 0
    fileType = '?'
    inodeLinkCount = 0
    realLinkCount = 0


def inode_allocation(csvFile):
    totalInodes = -1
    firstInode = -1

    freeInodes = set([])
    #key is the inode number
    #value is class
    allocatedInodes = {}

    for row in csvFile:
        if (row[0] == "SUPERBLOCK"):
            totalInodes = int(row[2])
            firstInode = int(row[7])
        if (row[0] == "IFREE"):
            freeInodes.add(int(row[1]));
        if (row[0] == "INODE"):
            inodeNum = int(row[1])
            if (allocatedInodes.has_key(inodeNum) == False):
                newInodeInfo = inodeInfo()
                newInodeInfo.isValid = 1
                newInodeInfo.fileType = row[2]
                newInodeInfo.inodeLinkCount = int(row[6])
                allocatedInodes[inodeNum] = newInodeInfo
            else:
                allocatedInodes[inodeNum].isValid = 1
                allocatedInodes[inodeNum].inodeLinkCount = int(row[6])
        if (row[0] == "DIRENT"):
            inodeNum = int(row[3])
            if (allocatedInodes.has_key(inodeNum) == False):
                newInodeInfo = inodeInfo()
                newInodeInfo.realLinkCount = 1
                allocatedInodes[inodeNum] = newInodeInfo
            else:
                allocatedInodes[inodeNum].realLinkCount = allocatedInodes[inodeNum].realLinkCount + 1
                
    #root directory special case check
    if 2 in freeInodes and allocatedInodes.has_key(2):
        print("ALLOCATED INODE 2 ON FREELIST")
    if 2 in freeInodes and allocatedInodes.has_key(2) == False:
        print("UNALLOCATED INODE 2 NOT ON FREELIST")
    if allocatedInodes[2].realLinkCount != allocatedInodes[2].inodeLinkCount:
        print("INODE 2 HAS {} LINKS BUT LINKCOUNT IS {}".format(allocatedInodes[2].realLinkCount, allocatedInodes[2].inodeLinkCount))

    for i in range(firstInode, totalInodes+1):
        if i in freeInodes and allocatedInodes.has_key(i) and allocatedInodes[i].isValid == 1:
            print("ALLOCATATED INODE {} ON FREELIST".format(i))
        if i not in freeInodes and allocatedInodes.has_key(i) == False:
            print("UNALLOCATED INODE {} NOT ON FREELIST".format(i))
        if allocatedInodes.has_key(i) == True and allocatedInodes[i].isValid == 1:
            if (allocatedInodes[i].realLinkCount != allocatedInodes[i].inodeLinkCount):
                print("INODE {} HAS {} LINKS BUT LINKCOUNT IS {}".format(i, allocatedInodes[i].realLinkCount, allocatedInodes[i].inodeLinkCount))

    directoryMap = {}

    for row in csvFile:
        if (row[0] == "DIRENT"):
            childInode = int(row[3])
            parentInode = int(row[1])
            inodeName =row[6]
            #invalid inode
            if (childInode < 1 or childInode > totalInodes):
                print("DIRECTORY INODE {} NAME {} INVALID INODE {}".format(parentInode, inodeName, childInode))
            elif (allocatedInodes.has_key(childInode) == True and allocatedInodes[childInode].isValid == 0):
                print("DIRECTORY INODE {} NAME {} UNALLOCATED INODE {}".format(parentInode, inodeName, childInode))
            # unallocated inode
            if (childInode not in freeInodes and allocatedInodes.has_key(childInode) == False):
                print("DIRECTORY INODE {} NAME {} UNALLOCATED INODE {}".format(parentInode, inodeName, childInode))
            if (inodeName != "." and inodeName != ".."):
                if (directoryMap.has_key(childInode) == False):
                    directoryMap[childInode] = parentInode

    for row in csvFile:
        if (row[0] == "DIRENT"):
            childInode = int(row[3])
            parentInode = int(row[1])
            if (row[6] == "'.'"):
                if (childInode != parentInode):
                    print("DIRECTORY INODE {} NAME {} LINK TO INODE {} SHOULD BE {}".format(parentInode, row[6], childInode, parentInode))
            if (row[6] == "'..'"):
                if (directoryMap[parentInode] != childInode):
                    print("DIRECTORY INODE {} NAME {} LINK TO INODE {} SHOULD BE {}".format(parentInode, row[6], childInode, parentInode))


def block_consistency(csvFile):

    #first determine legal blocks
    inodeSize = -1
    totalBlocks = -1
    totalInodes = -1
    blockSize = -1

    bitMap = -1
    inodeMap = -1
    inodeTable = -1
    endOfInodeTable = -1

    for row in csvFile:
        if (row[0] == "SUPERBLOCK"):
            totalBlocks = int(row[1])
            totalInodes = int(row[2])
            blockSize = int(row[3])
            inodeSize = int(row[4])
        if (row[0] == "GROUP"):
            bitMap = int(row[6])
            inodeMap = int(row[7])
            inodeTable = int(row[8])

    startOfDataBlocks = inodeTable + (totalInodes * inodeSize / blockSize)

    freeBlocks = set([])
    #key is the block number                                                                                                                                                
    #value is a list of classes
    allocatedBlocks = {}

    #CREATING DATA STRUCTURES
    for row in csvFile:
        if (row[0] == "BFREE"):
            freeBlocks.add(int(row[1]))
        if (row[0] == "INODE"):
            offset = 0
            for i in range (12, 27):
                blockNum = int(row[i])
                if (blockNum != 0):
                    if (blockNum < 0 or blockNum > (totalBlocks-1)):
                        if (i < 24):
                            print("INVALID BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset))
                        if (i == 24):
                            print("INVALID INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset + 12))
                        if (i == 25):
                            print("INVALID DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset + 12 + 256))
                        if (i == 26):
                            print("INVALID TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset + 12 + 256* 256 + 256))
                    if (blockNum < startOfDataBlocks):
                        if (i < 24):
                            print ("RESERVED BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset))
                        if (i == 24):
                            print ("RESERVED INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset+12))
                        if (i == 25):
                            print ("RESERVED DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset+12+256))
                        if (i == 26):
                            print ("RESERVED TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], offset+12+256*256 + 256))
                    else:
                        newBlockInfo = blockInfo()
                        newBlockInfo.inode = int(row[1])
                        if (i < 24):
                            indir = 0
                            offset_to_add = offset
                        if (i == 24):
                            indir = 1
                            offset_to_add = offset + 12
                        if (i == 25):
                            indir = 2
                            offset_to_add = offset + 12 + 256
                        if (i == 26):
                            indir = 3
                            offset_to_add = offset + 12 + 256 + 256 * 256
                        newBlockInfo.indirection = indir
                        newBlockInfo.offsets = offset_to_add
                        if (allocatedBlocks.has_key(blockNum) == False):
                            allocatedBlocks[blockNum] = [newBlockInfo]                            
                        else:
                            allocatedBlocks[blockNum].append(newBlockInfo)
                    offset = offset + 1
        if (row[0] == "INDIRECT"):
            blockNum = int(row[4])
            if (blockNum < 0 or blockNum > (totalBlocks-1)):
                if (row[2] == "1"):
                    print("INVALID INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], row[3]))
                if (row[2] == "2"):
                    print("INVALID DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], row[3]))
                if (row[2] == "3"):
                    print("INVALID TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], row[3]))
                continue
            elif (blockNum < endOfInodeTable):
                if (row[2] == "1"):
                    print("RESERVED INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], row[3]))
                if (row[2] == "2"):
                    print("RESERVED DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], row[3]))
                if (row[2] == "3"):
                    print("RESERVED TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(blockNum, row[1], row[3]))
            else:
                indir = int(row[2])
                if (indir == 1):
                    offset = row[3]
                if (indir ==  2):
                    offset = row[3]
                if (indir == 3):
                    offset = row[3]
                newBlockInfo = blockInfo()
                newBlockInfo.inode = int(row[1])
                newBlockInfo.indirection = indir
                newBlockInfo.offsets = offset
                blockNum = int(row[5])
                if (allocatedBlocks.has_key(blockNum) == False):
                    allocatedBlocks[blockNum] = [newBlockInfo]
                else:
                    allocatedBlocks[blockNum].append(newBlockInfo)

    # allocated and unreferenced blocks
    for i in range(startOfDataBlocks, totalBlocks):
        if i in freeBlocks and allocatedBlocks.has_key(i):
            print("ALLOCATED BLOCK {} ON FREELIST".format(i));
        if i not in freeBlocks and allocatedBlocks.has_key(i) == False:
            print("UNREFERENCED BLOCK {}".format(i)) 
        # checking for duplicates
        if (allocatedBlocks.has_key(i) == True):
            if (len(allocatedBlocks[i]) > 1):
                for j in range(len(allocatedBlocks[i])):
                    indir = allocatedBlocks[i][j].indirection
                    if (indir == 0):
                        print("DUPLICATE BLOCK {} IN INODE {} AT OFFSET {}".format(i, allocatedBlocks[i][j].inode, allocatedBlocks[i][j].offsets))
                    if (indir == 1):
                        print("DUPLICATE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(i, allocatedBlocks[i][j].inode, allocatedBlocks[i][j].offsets))
                    if (indir == 2):
                        print("DUPLICATE DOUBLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(i, allocatedBlocks[i][j].inode, allocatedBlocks[i][j].offsets))
                    if (indir == 3):
                        print("DUPLICATE TRIPLE INDIRECT BLOCK {} IN INODE {} AT OFFSET {}".format(i, allocatedBlocks[i][j].inode, allocatedBlocks[i][j].offsets))
            

def main():
    if len(sys.argv) != 2:
        print("Wrong number of arguments")
        sys.exit()
    
    csvFile = []

    with open(sys.argv[1], 'r') as csvfile:
        reader = csv.reader(csvfile)
        for row in reader:
            csvFile.append(row)
        
    block_consistency(csvFile)
    inode_allocation(csvFile)
    

if __name__ == "__main__":
    main()
