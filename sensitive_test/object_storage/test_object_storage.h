/*
 * Copyright (c) 2025 OceanBase.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OCEANBASE_UNITTEST_OBJECT_STORAGE_H_
#define OCEANBASE_UNITTEST_OBJECT_STORAGE_H_

#include "lib/utility/ob_print_utils.h"
#include "lib/hash/ob_hashmap.h"
#include "gtest/gtest.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include "lib/restore/ob_storage.h"
#include "lib/utility/ob_test_util.h"
#include "lib/restore/ob_storage_info.h"

namespace oceanbase
{
namespace unittest
{

struct Config
{
  Config(const char *storage_type = nullptr, const char *bucket = nullptr,
         const char *endpoint = nullptr, const char *ak = nullptr, const char *sk = nullptr,
         const char *region = nullptr, const char *appid = nullptr,
         const char *extension = nullptr, const char *role_arn = nullptr, const char *external_id = nullptr,
         const bool enable_obdal = false)
      : storage_type_(storage_type), bucket_(bucket), endpoint_(endpoint), ak_(ak), sk_(sk),
        region_(region), appid_(appid), extension_(extension), role_arn_(role_arn), external_id_(external_id), enable_obdal_(enable_obdal)
  {}
  TO_STRING_KV(K_(storage_type), K_(bucket), K_(endpoint), K_(ak), KP_(sk), K_(region), K_(appid), K_(extension), KP_(role_arn), KP_(external_id), K_(enable_obdal));

  const char *storage_type_;
  const char *bucket_;
  const char *endpoint_;
  const char *ak_;
  const char *sk_;
  const char *region_;
  const char *appid_;
  const char *extension_;
  const char *role_arn_;
  const char *external_id_;
  bool enable_obdal_;
};

class ObObjectStorageUnittestCommon
{
public:
  ObObjectStorageUnittestCommon()
  {
    MEMSET(dir_uri_, 0, sizeof(dir_uri_));
  }
  virtual ~ObObjectStorageUnittestCommon() {}

  static int set_storage_info(const char *bucket, const char *endpoint,
      const char *secretid, const char *secretkey, const char *appid, const char *region,
      const char *extension, ObObjectStorageInfo &info_base)
  {
    int ret = OB_SUCCESS;
    char account[OB_MAX_URI_LENGTH] = { 0 };
    ObStorageType storage_type = ObStorageType::OB_STORAGE_MAX_TYPE;
    if (OB_FAIL(get_storage_type_from_path(bucket, storage_type))) {
      OB_LOG(WARN, "fail to get storage type", K(ret), K(bucket));
    } else if (OB_UNLIKELY(ObStorageType::OB_STORAGE_MAX_TYPE == storage_type)) {
      ret = OB_INVALID_ARGUMENT;
      OB_LOG(WARN, "storage type invalid", K(ret), K(storage_type), K(bucket));
    }
  
    if (OB_FAIL(ret)) {
    } else if (storage_type == ObStorageType::OB_STORAGE_FILE) {
      if (OB_FAIL(info_base.set(storage_type, account))) {
        OB_LOG(WARN, "fail to set storage info", K(ret), K(storage_type));
      }
    } else {
      int64_t pos = 0;
      if (OB_FAIL(databuff_printf(account, sizeof(account), pos,
                                  "host=%s&access_id=%s&access_key=%s",
                                  endpoint, secretid, secretkey))) {
        OB_LOG(WARN, "fail to set account", K(ret), K(endpoint));
      }

      if (OB_SUCC(ret) && OB_NOT_NULL(extension)) {
        if (OB_FAIL(databuff_printf(account, sizeof(account), pos, "&%s", extension))) {
          OB_LOG(WARN, "fail to set extension", K(ret), K(pos), K(extension));
        }
      }

      if (OB_SUCC(ret) && OB_FAIL(info_base.set(storage_type, account))) {
        OB_LOG(WARN, "fail to set storage info", K(ret), K(storage_type)); 
      }
    }
    return ret;
  }

  static bool need_skip_test(const ObString &value)
  {
    return value.prefix_match("##");
  }

  static int set_storage_info_with_assume_role (const char *bucket, 
      const char *endpoint, const char *appid, const char *region, 
      const char *extension, const char *role_arn, const char *external_id, 
      ObObjectStorageInfo &info_base) 
  {
      int ret = OB_SUCCESS;
      char account[OB_MAX_URI_LENGTH] = {0};
      ObStorageType storage_type = ObStorageType::OB_STORAGE_MAX_TYPE;
      if (OB_ISNULL(role_arn) || OB_ISNULL(endpoint) || OB_ISNULL(bucket)) {
        ret = OB_INVALID_ARGUMENT;
        OB_LOG(WARN, "invalid argument", K(ret), KP(role_arn), K(endpoint), K(bucket));
      } else if (OB_FAIL(get_storage_type_from_path(bucket, storage_type))) {
        OB_LOG(WARN, "fail to get storage type", K(ret), K(bucket));
      } else if (OB_UNLIKELY(ObStorageType::OB_STORAGE_MAX_TYPE == storage_type)) {
        ret = OB_INVALID_ARGUMENT;
        OB_LOG(WARN, "storage type invalid", K(ret), K(storage_type), K(bucket));
      } else if (storage_type == ObStorageType::OB_STORAGE_FILE) {
        ret = OB_NOT_SUPPORTED;
        OB_LOG(WARN, "nfs not support assume role mode", K(ret), K(storage_type));
      } else {
        int64_t pos = 0;
        if (OB_FAIL(databuff_printf(account, sizeof(account), pos,
                                  "host=%s&role_arn=%s",
                                  endpoint, role_arn))) {
          OB_LOG(WARN, "fail to set account", K(ret), K(endpoint));
        } else if (OB_NOT_NULL(external_id) && !need_skip_test(external_id)) {
          if (OB_FAIL(databuff_printf(account, sizeof(account), pos, "&external_id=%s", external_id))) {
            OB_LOG(WARN, "fail to set external_id", K(ret), K(pos), KP(external_id));
          }
        }

        if (OB_SUCC(ret) && OB_NOT_NULL(extension)) {
          if (OB_FAIL(databuff_printf(account, sizeof(account), pos, "&%s", extension))) {
            OB_LOG(WARN, "fail to set extension", K(ret), K(pos), K(extension));
          }
        }

        if (OB_SUCC(ret) && OB_FAIL(info_base.set(storage_type, account))) {
          OB_LOG(WARN, "fail to set storage info", K(ret), K(storage_type));
        } else {
          OB_LOG(INFO, "set storage info with assume role", K(account));
        }
      }
      return ret;

  }
protected:
  char dir_uri_[OB_MAX_URI_LENGTH];
  ObObjectStorageInfo info_base_;
  bool enable_test_;
  bool access_by_assume_role_;
};

class ObObjectStorageUnittestUtil
{
public:
  static std::string custom_test_name(const ::testing::TestParamInfo<Config> &info)
  {
    std::string name = info.param.storage_type_;
    if (info.param.enable_obdal_) {
      name = "ObDal_" + name;
    }
    name += "_with_extension_";
    name += (OB_NOT_NULL(info.param.extension_) ? info.param.extension_ : "none");
    std::replace(name.begin(), name.end(), '-', '_');
    std::replace(name.begin(), name.end(), '=', '_');
    return name;
  }

  static void generate_configs_with_checksum(std::vector<Config> &configs,
      const Config &base_config, const std::vector<const char*> &checksum_types)
  {
    for (const char *checksum_type : checksum_types) {
      configs.push_back(Config(base_config.storage_type_, base_config.bucket_,
                               base_config.endpoint_, base_config.ak_, base_config.sk_,
                               base_config.region_, base_config.appid_, checksum_type, 
                               base_config.role_arn_, base_config.external_id_, base_config.enable_obdal_));
    }
  }

  static void disrupt_network(const int64_t sleep_time_s)
  {
    ASSERT_GT(sleep_time_s, 0);
    // Clean up existing rules (to avoid conflicts)
    system("tc qdisc del dev eth0 root || true");
    // Add tc rule to set packet loss rate
    // Add root qdisc on eth0, identified with handle 1:, type htb (Hierarchical Token Bucket), default class 30
    system("tc qdisc add dev eth0 root handle 1: htb default 30");
    // Add a subclass 1:1 under parent class 1:, with a rate limit of 10000mbit/s
    system("tc class add dev eth0 parent 1: classid 1:1 htb rate 10000mbit");
    // Under the 1:1 class, add another subclass 1:2 with a rate limit of 10000mbit/s
    system("tc class add dev eth0 parent 1: classid 1:2 htb rate 10000mbit");
    // Add cgroup filter, match all protocols, priority is 1
    system("tc filter add dev eth0 parent 1: protocol all prio 1 handle 1: cgroup");
    // Attach a netem queue discipline to subclass 1:1, set packet loss rate to 100%
    system("tc qdisc add dev eth0 parent 1:1 handle 10: netem loss 100%");
    system("tc qdisc add dev eth0 parent 1:2 handle 20: sfq perturb 10");
    // Process PID
    int pid = getpid();
    std::string cgroupPath = "/sys/fs/cgroup/net_cls/my_cgroup";
    // Create a cgroup named my_cgroup, apply to the net_cls subsystem (used for network classification)
    system("cgcreate -g net_cls:/my_cgroup");
    // Set the classid of the net_cls subsystem to 1:1, subclass 1:1 will enter the qdisc corresponding to 10:, thus causing 100% packet loss for this cgroup
    system("echo 0x00010001 | tee /sys/fs/cgroup/net_cls/my_cgroup/net_cls.classid > /dev/null");
    // Add current process to cgroup
    std::string command = std::string("cgclassify -g net_cls:my_cgroup ") + std::to_string(pid);
    system(command.c_str());
    std::cout << "Network disruption begin, rules created." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(sleep_time_s));
    // cleanup
    system("echo 0 | tee /sys/fs/cgroup/net_cls/my_cgroup/net_cls.classid");
    system("tc qdisc del dev eth0 root");
    command = std::string("rmdir ") + cgroupPath;
    system(command.c_str());
    std::cout << "Network disruption finished, rules cleared." << std::endl;
  }
};

class TestObjectStorage : public ::testing::TestWithParam<Config>,
                          public ObObjectStorageUnittestCommon
{
public:
  TestObjectStorage() {}
  virtual ~TestObjectStorage() {}

  virtual void SetUp() override
  {
    const Config &config = GetParam();
    if (need_skip_test(config.ak_) || need_skip_test(config.sk_)) {
      enable_test_ = false;
    } else {
      enable_test_ = true;
    }
    if (nullptr == config.extension_) {
      with_checksum_ = false;
    } else {
      with_checksum_ = true;
    }

    if (enable_test_) {
      OK(set_storage_info(config.bucket_, config.endpoint_, config.ak_,
                          config.sk_, config.appid_, config.region_,
                          config.extension_, info_base_));
      if (info_base_.get_type() == ObStorageType::OB_STORAGE_FILE) {
        OK(databuff_printf(dir_uri_, sizeof(dir_uri_), "%s%s/",
                           OB_FILE_PREFIX, get_current_dir_name()));
      } else {
        OK(databuff_printf(dir_uri_, sizeof(dir_uri_), "%s/%s/", config.bucket_, dir_name_));
        OK(databuff_printf(bucket_, sizeof(bucket_), "%s/", config.bucket_));
      }
    }
    if (config.enable_obdal_) {
      cluster_enable_obdal_config = &ObClusterEnableObdalConfigBase::get_instance();
    } else {
      cluster_enable_obdal_config = nullptr;
    }
  }

  virtual void TearDown() override
  {
    if (info_base_.get_type() == ObStorageType::OB_STORAGE_FILE) {
      ObStorageUtil util;
      bool is_exist = false;
      OK(util.open(&info_base_));
      OK(util.is_exist(dir_uri_, is_exist));

      if (is_exist) {
        std::string dir_path = (dir_uri_ + strlen(OB_FILE_PREFIX));
        if (dir_path.size() > strlen(get_current_dir_name()) + 1) {
          std::string s = "rm -rf ";
          s += dir_path;
          ASSERT_EQ(0, ::system(s.c_str()));
        }
      }
    }
    info_base_.reset();
  }

  static void SetUpTestCase()
  {
    OK(init_s3_env());
    OK(init_obdal_env());
    int64_t cur_ts = ObTimeUtility::current_time();
    OK(databuff_printf(dir_name_, sizeof(dir_name_), "object_storage_unittest_dir_%ld", cur_ts));
    // register base mgr for support azblob
    OK(ObObjectStorageInfo::register_cluster_version_mgr(&ObClusterVersionBaseMgr::get_instance()));
  }
  static void TearDownTestCase()
  {
    fin_s3_env();
    fin_obdal_env();
  }

protected:
  static char dir_name_[OB_MAX_URI_LENGTH];
  char bucket_[64];
  static char uri[OB_MAX_URI_LENGTH];
  bool with_checksum_;
};


class TestObjectStorageListOp : public ObBaseDirEntryOperator
{
public:
  TestObjectStorageListOp(const int64_t expect_file_size = -1) : expect_file_size_(expect_file_size) {}
  virtual ~TestObjectStorageListOp() {}
  
  int func(const dirent *entry) override
  {
    int ret = OB_SUCCESS;
    OB_LOG(INFO, "TestObjectStorageListOp list file", K(entry->d_name));
    const int64_t size = get_size();
    if (expect_file_size_ != -1 && size != expect_file_size_) {
      ret = OB_ERR_UNEXPECTED;
      OB_LOG(WARN, "wrong file size",
          K_(expect_file_size), K(size), K(entry->d_name));
    } else {
      std::string tmp(entry->d_name);
      object_names_.emplace(std::move(tmp));
      object_list_.push_back(entry->d_name);
    }
    return ret;
  }
  virtual bool need_get_file_size() const { return true; }

  std::set<std::string> object_names_;
  std::vector<std::string> object_list_;
  const int64_t expect_file_size_;
};

} // end of unittest
} // end of oceanbase

#endif
