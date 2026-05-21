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

#include <gtest/gtest.h>
#define protected public
#define private public
#include "test_object_storage.h"
#include "object_storage_authorization_info.h"
#include "mittest/mtlenv/mock_tenant_module_env.h"
#undef private
#undef protected

namespace oceanbase
{
namespace unittest
{

using namespace oceanbase::common;

class TestObAdmin : public ::testing::TestWithParam<Config>, public ObObjectStorageUnittestCommon
{ 
public:
  TestObAdmin() {}
  virtual ~TestObAdmin(){}
  virtual void SetUp() override
  {
    const Config &config = GetParam();
    if (need_skip_test(config.ak_) || need_skip_test(config.sk_)) {
      enable_test_ = false;
    } else {
      enable_test_ = true;
    }
    
    if (ObString(config.role_arn_).prefix_match("##") || ObString(config.role_arn_).empty()) {
      enable_assume_role_ = false;
    } else {
      enable_assume_role_ = true;
    }

    if (enable_test_ || enable_assume_role_) {
      ASSERT_EQ(OB_SUCCESS, set_storage_info(config.bucket_, config.endpoint_, config.ak_,
                                             config.sk_, config.appid_, config.region_,
                                             config.extension_, info_base_));
      if (info_base_.get_type() == ObStorageType::OB_STORAGE_FILE) {
        ASSERT_EQ(OB_SUCCESS, databuff_printf(dir_uri_, sizeof(dir_uri_), "%s%s/%s/",
                                              OB_FILE_PREFIX, get_current_dir_name(), dir_name_));
      } else {
        ASSERT_EQ(OB_SUCCESS,
            databuff_printf(dir_uri_, sizeof(dir_uri_), "%s/%s/", config.bucket_, dir_name_));
      }
    }

    if (config.enable_obdal_) {
      enable_obdal_ = true;
    }
  }
  virtual void TearDown() override
  {
    info_base_.reset();
  }

  static void SetUpTestCase()
  {
    ASSERT_EQ(OB_SUCCESS, ObObjectStorageInfo::register_cluster_version_mgr(
      &ObClusterVersionBaseMgr::get_instance()));
    char uri[OB_MAX_URI_LENGTH] = { 0 };
    ASSERT_EQ(OB_SUCCESS, databuff_printf(uri, sizeof(uri), "%s%s/%s/",
                                          OB_FILE_PREFIX, get_current_dir_name(), dir_name_));
    mkdir_if_not_exist(uri);
    
    ASSERT_EQ(OB_SUCCESS, databuff_printf(uri, sizeof(uri), "%s%s/%s/",
                                          OB_FILE_PREFIX, get_current_dir_name(), ob_admin_log_dir_));
    mkdir_if_not_exist(uri);
    std::string cur_path = get_current_dir_name();
    ASSERT_EQ(OB_SUCCESS, MockTenantModuleEnv::get_instance().init());
    // MockTenantModuleEnv will change the current path, so we have to set it back.
    ASSERT_EQ(OB_SUCCESS, chdir(cur_path.c_str()));
    omt::ObTenantConfigGuard tenant_config(TENANT_CONF(MTL_ID()));
    ASSERT_TRUE(tenant_config.is_valid());
    tenant_config->sts_credential = oceanbase::unittest::STS_CREDENTIAL;
  }
  static void TearDownTestCase() {
    MockTenantModuleEnv::get_instance().destroy();
  }

  static void mkdir_if_not_exist(const char *uri)
  {
    ObStorageUtil util;
    ObObjectStorageInfo info;
    ASSERT_EQ(OB_SUCCESS, info.set(ObStorageType::OB_STORAGE_FILE, ""));
    ASSERT_EQ(OB_SUCCESS, util.open(&info));
    bool is_exist = false;
    ASSERT_EQ(OB_SUCCESS, util.is_exist(uri, is_exist));
    if (!is_exist) {
      ASSERT_EQ(OB_SUCCESS, util.mkdir(uri));
    }
  }

  void run_cmd(const char *cmd)
  {
    // cmd include sk, cannot print directly
    std::cout << "---------------------------- Run cmd ----------------------------" << std::endl;
    ASSERT_NE(nullptr, cmd);
    int result = system(cmd);
    if (result == -1) {
      ASSERT_TRUE(false) << "system call failed";
    } else if (WIFEXITED(result) && WEXITSTATUS(result) != 0) {
      ASSERT_TRUE(false) << "Command failed with exit status " << WEXITSTATUS(result);
    }
  }
  
  void construct_and_run_cmd(ObObjectStorageInfo &info, std::string dir_uri);

public:
  bool enable_assume_role_;
  bool enable_obdal_;

protected:
  static const char *dir_name_;
  static const char *ob_admin_log_dir_;
};

const char *TestObAdmin::dir_name_ = "object_storage_ob_admin_dir";
const char *TestObAdmin::ob_admin_log_dir_ = "tmp_log";


std::string remove_appid(std::string input) {
  std::string key = "appid=";
  size_t pos = input.find(key);
    
  if (pos != std::string::npos) {
    size_t end_pos = input.find('&', pos);
    if (end_pos != std::string::npos) {
      input.erase(pos, end_pos - pos + 1);
    } else {
      if (pos > 0 && input[pos - 1] == '&') {
        input.erase(pos - 1);
      } else {
        input.erase(pos);
      }
    }
  }
    
  return input;
}

void TestObAdmin::construct_and_run_cmd(
    ObObjectStorageInfo &info, 
    std::string dir_uri)
{
  char uri[OB_MAX_URI_LENGTH] = {0};
  ASSERT_EQ(OB_SUCCESS, databuff_printf(uri, sizeof(uri), "export OB_ADMIN_LOG_DIR=%s/%s",
                                          get_current_dir_name(), ob_admin_log_dir_));
  std::string cmd = uri;
  cmd += " && ./ob_admin test_io_device -d'";
  cmd += dir_uri;
  cmd += "' ";
  if (info.get_type() != ObStorageType::OB_STORAGE_FILE) {
    MEMSET(uri, 0, sizeof(uri));
    ASSERT_EQ(OB_SUCCESS, info.get_storage_info_str(uri, sizeof(uri)));
    cmd += "-s'";
    cmd += uri;
    cmd += "' ";
    if (info.is_assume_role_mode()) {
      cmd += " -i'";
      cmd += oceanbase::unittest::STS_CREDENTIAL;
      cmd += "' ";
    }
  }

  if (enable_obdal_) {
    cmd += " -a ";
  }

  {
    std::string tmp_cmd = cmd + " -f 2 ";
    run_cmd(tmp_cmd.c_str());
  }
}


TEST_P(TestObAdmin, test_ob_admin)
{
  int ret = OB_SUCCESS;
  if (enable_test_) {
    construct_and_run_cmd(info_base_, dir_uri_);
  }
}

TEST_P(TestObAdmin, test_ob_admin_with_obdal)
{
  int ret = OB_SUCCESS;
  if (enable_test_) {
    enable_obdal_ = true;
    construct_and_run_cmd(info_base_, dir_uri_);
    enable_obdal_ = false;
  }
}

TEST_P(TestObAdmin, test_ob_admin_with_assume) 
{
  int ret = OB_SUCCESS;
  if (info_base_.get_type() != ObStorageType::OB_STORAGE_FILE && enable_assume_role_) {
    info_base_.reset();
    const Config &config = GetParam();
    ASSERT_EQ(OB_SUCCESS, set_storage_info_with_assume_role(config.bucket_, config.endpoint_, 
                                                            config.appid_, config.region_,
                                                            config.extension_, config.role_arn_, 
                                                            config.external_id_, info_base_));
    construct_and_run_cmd(info_base_, dir_uri_);
    
  }
}

TEST_P(TestObAdmin, test_ob_admin_with_enable_worm)
{
  int ret = OB_SUCCESS;
  if ((enable_test_ || enable_assume_role_) && ObStorageChecksumType::OB_MD5_ALGO == info_base_.get_checksum_type()) {
    char extension[OB_MAX_BACKUP_EXTENSION_LENGTH] = {0};
    const Config &config = GetParam();
    int64_t pos = 0;
    ASSERT_EQ(OB_SUCCESS, databuff_printf(extension, OB_MAX_BACKUP_EXTENSION_LENGTH, 
                              pos, "%s&%s", config.extension_, "enable_worm=true"));
    if (enable_test_) {
      info_base_.reset();
      ASSERT_EQ(OB_SUCCESS, set_storage_info(config.bucket_,
        config.endpoint_,
        config.ak_, config.sk_,
        config.appid_, config.region_,
        extension, info_base_));
      construct_and_run_cmd(info_base_, dir_uri_);
    }        
    if (enable_assume_role_) {
      info_base_.reset();
      ASSERT_EQ(OB_SUCCESS, set_storage_info_with_assume_role(config.bucket_, config.endpoint_, 
        config.appid_, config.region_,
        extension, config.role_arn_, 
        config.external_id_, info_base_));
      construct_and_run_cmd(info_base_, dir_uri_);
    }
  }
}

std::vector<Config> all_configs;
INSTANTIATE_TEST_CASE_P(
  ConfigCombinations,
  TestObAdmin,
  ::testing::ValuesIn(all_configs),
  ObObjectStorageUnittestUtil::custom_test_name
);

} // end namaspace unittest
} // end namespace oceanbase

int main(int argc, char **argv)
{
  oceanbase::unittest::ObObjectStorageUnittestUtil::generate_configs_with_checksum(
    oceanbase::unittest::all_configs,
    oceanbase::unittest::Config {
      "FILE",                             /*storage_type*/
      "file://",                          /*bucket*/
      nullptr,                            /*enpoint*/
      nullptr,                            /*ak*/
      nullptr,                            /*sk*/
      nullptr,                            /*region*/
      nullptr,                            /*appid*/
      nullptr,                            /*entension*/
    },
    {nullptr}
  );
  oceanbase::unittest::ObObjectStorageUnittestUtil::generate_configs_with_checksum(
    oceanbase::unittest::all_configs,
    oceanbase::unittest::Config{
        "S3",                               /*storage_type*/
        oceanbase::unittest::S3_BUCKET,     /*bucket*/
        oceanbase::unittest::S3_ENDPOINT,   /*enpoint*/
        oceanbase::unittest::S3_AK,         /*ak*/
        oceanbase::unittest::S3_SK,         /*sk*/
        oceanbase::unittest::S3_REGION,     /*region*/
        nullptr,                            /*appid*/
        nullptr,                            /*entension*/
        oceanbase::unittest::S3_ROLE_ARN,   /*role_arn*/
        oceanbase::unittest::S3_EXTERNAL_ID /*external_id*/
    },
    {nullptr, "checksum_type=md5", "checksum_type=crc32"}
  );
  oceanbase::unittest::ObObjectStorageUnittestUtil::generate_configs_with_checksum(
    oceanbase::unittest::all_configs,
    oceanbase::unittest::Config{
        "OBS",                               /*storage_type*/
        oceanbase::unittest::OBS_BUCKET,     /*bucket*/
        oceanbase::unittest::OBS_ENDPOINT,   /*enpoint*/
        oceanbase::unittest::OBS_AK,         /*ak*/
        oceanbase::unittest::OBS_SK,         /*sk*/
        nullptr,                             /*region*/
        nullptr,                             /*appid*/
        nullptr,                             /*entension*/
        oceanbase::unittest::OBS_ROLE_ARN,   /*role_arn*/
        oceanbase::unittest::OBS_EXTERNAL_ID /*external_id*/
    },
    {nullptr, "checksum_type=md5"}
  );
  oceanbase::unittest::ObObjectStorageUnittestUtil::generate_configs_with_checksum(
    oceanbase::unittest::all_configs,
    oceanbase::unittest::Config {
      "GCS",                              /*storage_type*/
      oceanbase::unittest::GCS_BUCKET,    /*bucket*/
      oceanbase::unittest::GCS_ENDPOINT,  /*enpoint*/
      oceanbase::unittest::GCS_AK,        /*ak*/
      oceanbase::unittest::GCS_SK,        /*sk*/
      nullptr,                            /*role_arn*/
      nullptr,                            /*external_id*/
    },
    {nullptr, "checksum_type=md5"}
  );
  oceanbase::unittest::ObObjectStorageUnittestUtil::generate_configs_with_checksum(
    oceanbase::unittest::all_configs,
    oceanbase::unittest::Config {
      "AZBLOB",                              /*storage_type*/
      oceanbase::unittest::AZBLOB_BUCKET,    /*bucket*/
      oceanbase::unittest::AZBLOB_ENDPOINT,  /*enpoint*/
      oceanbase::unittest::AZBLOB_AK,        /*ak*/
      oceanbase::unittest::AZBLOB_SK,        /*sk*/
      nullptr,                            /*role_arn*/
      nullptr,                            /*external_id*/
    },
    {nullptr}
  );
  system("rm -f test_ob_admin.log*");
  OB_LOGGER.set_file_name("test_ob_admin.log", true, true);
  OB_LOGGER.set_log_level("DEBUG");
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
