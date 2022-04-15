# Derecho_test

这是一个简单地使用[Derecho](https://github.com/Derecho-Project/derecho)进行吞吐量测试的实验，用于验证共识协议对吞吐量的影响。


## Environment
支持**RoCE**网卡的机器若干。

每台机器：
* 安装Derecho
```
git clone https://github.com/Derecho-Project/derecho.git
cd derecho
./build.sh Release
cd build-Release
make install
```
* 添加环境变量
```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
```
* clone本实验
```
cd ~
git clone https://github.com/River861/derecho_test.git
cd derecho_test
```

## Config
* 结点配置
    参考配置在`/sample_config`中
    * leader节点（只有一个）
    ```
    cp ./sample_config/derecho.cfg derecho.cfg
    ```
    * 其它节点
    ```
    cp ./sample_config/derecho_other.cfg derecho.cfg
    ```
    * 所有配置文件记得修改 `leader_ip`、`local_ip`、`local_id`(leader为0，其它节点从1开始递增)

* 测试配置
    * 修改`main.cpp` (`main_bk.cpp`、`repeater_rpc_test.cpp`同理)
    ```cpp
    const int num_clients = 8;          // clients总数目（按进程算）
    const int shard_size = 2;           // 也就是replica factor
    const double test_time = 10.0;      // 测试时间
    const int msg_size = 16;            // 消息大小
    const int total_msg_num = 10000;    // 消息数目
    ```
    * 修改`run.py`
    ```python
    clients_num = 8                      # 每个结点跑的client数目（也就是进程数）
    ```

## Run
* 编译（三选一）
```shell
make：运行10秒来测试吞吐量
make bk：运行10000条消息来测试吞吐量
make test：debug
```

* 执行（所有结点执行）
```shell
run.py
```

* 结果（所有结点执行）
```shell
python3 get_sum.py
```
然后依次收集所有结点的结果相加即为总吞吐量。
