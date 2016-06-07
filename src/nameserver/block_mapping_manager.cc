// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "block_mapping_manager.h"

namespace baidu {
namespace bfs {

BlockMappingManager::BlockMappingManager(int32_t bucket_num) :
    blockmapping_bucket_num_(bucket_num) {
    block_mapping_.resize(blockmapping_bucket_num_);
    for (size_t i = 0; i < block_mapping_.size(); i++) {
        block_mapping_[i] = new BlockMapping();
    }
}

BlockMappingManager::~BlockMappingManager() {
}

int32_t BlockMappingManager::GetBucketOffset(int64_t block_id) {
    return block_id % blockmapping_bucket_num_;
}

bool BlockMappingManager::GetBlock(int64_t block_id, NSBlock* block) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    return block_mapping_[bucket_offset]->GetBlock(block_id, block);
}

bool BlockMappingManager::GetLocatedBlock(int64_t block_id, std::vector<int32_t>* replica, int64_t* block_size) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    return block_mapping_[bucket_offset]->GetLocatedBlock(block_id, replica, block_size);
}

bool BlockMappingManager::ChangeReplicaNum(int64_t block_id, int32_t replica_num) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    return block_mapping_[bucket_offset]->ChangeReplicaNum(block_id, replica_num);
}

void BlockMappingManager::AddNewBlock(int64_t block_id, int32_t replica,
                 int64_t version, int64_t block_size,
                 const std::vector<int32_t>* init_replicas) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    block_mapping_[bucket_offset]->AddNewBlock(block_id, replica, version, block_size, init_replicas);
}

bool BlockMappingManager::UpdateBlockInfo(int64_t block_id, int32_t server_id, int64_t block_size,
                     int64_t block_version) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    return block_mapping_[bucket_offset]->UpdateBlockInfo(block_id, server_id, block_size, block_version);
}

void BlockMappingManager::RemoveBlocksForFile(const FileInfo& file_info) {
    for (int i = 0; i < file_info.blocks_size(); i++) {
        int32_t bucket_offset = GetBucketOffset(file_info.blocks(i));
        block_mapping_[bucket_offset]->RemoveBlocksForFile(file_info);
    }
}

void BlockMappingManager::RemoveBlock(int64_t block_id) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    block_mapping_[bucket_offset]->RemoveBlock(block_id);
}

void BlockMappingManager::DealWithDeadNode(int32_t cs_id, const std::set<int64_t>& blocks) {
    std::vector<std::set<int64_t> > blocks_array;
    blocks_array.resize(block_mapping_.size());
    for (std::set<int64_t>::iterator it = blocks.begin(); it != blocks.end(); ++it) {
        int32_t bucket_offset = GetBucketOffset(*it);
        blocks_array[bucket_offset].insert(*it);
    }
    for (size_t i = 0; i < blocks_array.size(); i++) {
        block_mapping_[i]->DealWithDeadNode(cs_id, blocks_array[i]);
    }
}

StatusCode BlockMappingManager::CheckBlockVersion(int64_t block_id, int64_t version) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    return block_mapping_[bucket_offset]->CheckBlockVersion(block_id, version);
}

void BlockMappingManager::PickRecoverBlocks(int32_t cs_id, int32_t block_num,
                       std::map<int64_t, std::set<int32_t> >* recover_blocks,
                       int32_t* hi_num) {
    int cur_check_num = 0;
    for (int i = 0; i < blockmapping_bucket_num_; i++) {
        int64_t lo_check_num = 0, hi_check_num = 0;
        block_mapping_[i]->GetStat(NULL, NULL, &lo_check_num, &hi_check_num, NULL, NULL);
        cur_check_num += (lo_check_num + hi_check_num);
    }
    block_num -= cur_check_num;
    for (int i = 0; i < blockmapping_bucket_num_ && (size_t)block_num > recover_blocks->size(); i++) {
        block_mapping_[i]->PickRecoverBlocks(cs_id, block_num - recover_blocks->size(), recover_blocks, "hi");
        *(hi_num) += recover_blocks->size();
    }
    for (int i = 0; i < blockmapping_bucket_num_ && (size_t)block_num > recover_blocks->size(); i++) {
        block_mapping_[i]->PickRecoverBlocks(cs_id, block_num - recover_blocks->size(), recover_blocks, "low");
    }
}

void BlockMappingManager::ProcessRecoveredBlock(int32_t cs_id, int64_t block_id) {
    int32_t bucket_offset = GetBucketOffset(block_id);
    block_mapping_[bucket_offset]->ProcessRecoveredBlock(cs_id, block_id);
}

void BlockMappingManager::GetCloseBlocks(int32_t cs_id, google::protobuf::RepeatedField<int64_t>* close_blocks) {
    for (size_t i = 0; i < block_mapping_.size(); i++) {
        block_mapping_[i]->GetCloseBlocks(cs_id, close_blocks);
    }
}

void BlockMappingManager::GetStat(int64_t* lo_recover_num, int64_t* hi_recover_num,
             int64_t* lo_pending, int64_t* hi_pending,
             int64_t* lost_num, int64_t* incomplete_num) {
    for (size_t i = 0; i < block_mapping_.size(); i++) {
        int64_t lr = 0, hr = 0, lp = 0, hp = 0, ln = 0, in = 0;
        block_mapping_[i]->GetStat(&lr, &hr, &lp, &hp, &ln, &in);
        *(lo_recover_num) += lr;
        *(hi_recover_num) += hr;
        *(lo_pending) += lp;
        *(hi_pending) += hp;
        *(lost_num) += ln;
        *(incomplete_num) += in;
    }
}

void BlockMappingManager::ListRecover(std::string* hi_recover, std::string* lo_recover, std::string* lost,
                 std::string* hi_check, std::string* lo_check, std::string* incomplete) {
    for (size_t i = 0; i < block_mapping_.size(); i++) {
        block_mapping_[i]->ListRecover(hi_recover, lo_recover, lost, hi_check, lo_check, incomplete);
    }
}

void BlockMappingManager::SetSafeMode(bool safe_mode) {
    for (size_t i = 0; i < block_mapping_.size(); i++) {
        block_mapping_[i]->SetSafeMode(safe_mode);
    }
}

} //namespace bfs
} //namespace baidu
