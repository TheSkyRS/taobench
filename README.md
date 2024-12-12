# Final Artifact: TAOBench

This is the final artifact document for our project, **"Distributed Storage System"**. We provide the codes for our system in this repo and instructions on how to test it. Notice that branch **alex1** is the integrated version, and **alex2** is the distributed version as described in our papers. Please only run experiments on these two branchs. **Please pull the repo** before you run the experiments in case we've made updates. Please contact us through emails in our report if you encounter any problems.

---

## Quick Review

- **Code Repository**: [TAOBench on GitHub](https://github.com/TheSkyRS/taobench) (Yonghao Lin's GitHub)
- **TAOBench Overview**: [OVERVIEW.md provided by the original author](OVERVIEW.md)
- **Cloudlab Profile**: [Final Cloudlab TAO Profile](https://www.cloudlab.us/p/EECSE6894/alex-tao)
- **Cloudlab Disk Image**: `urn:publicid:IDN+utah.cloudlab.us+image+eecse6894-PG0:alex-tao-final:0`

### 1. Basic Information

#### 1.1 TAO: Facebook’s Distributed Data Store for the Social Graph
- **Paper**: [TAO Paper](https://www.usenix.org/system/files/conference/atc13/atc13-bronson.pdf)
- **Meta Blog**: [Meta TAO Blog](https://engineering.fb.com/2013/06/25/core-infra/tao-the-power-of-the-graph/)

#### 1.2 TAOBench
- **Code Repository**: [TAOBench GitHub](https://github.com/TheSkyRS/taobench) (Yonghao Lin's GitHub)
- **Paper**: [TAOBench Paper](https://www.vldb.org/pvldb/vol15/p1965-cheng.pdf)
- **Official Website**: [TAOBench.org](https://taobench.org/)
- **Meta Blog**: [TAOBench Blog](https://engineering.fb.com/2022/09/07/core-infra/taobench/)

#### 1.3 DCPerf/taobench
- **Code Repository**: [DCPerf GitHub](https://github.com/AlexWFreeman/Data-Center-Processing) (Xiaojie Wu's GitHub)
- **Meta Blog**: [DCPerf Meta Blog](https://engineering.fb.com/2024/08/05/data-center-engineering/dcperf-open-source-benchmark-suite-for-hyperscale-compute-applications/)

#### 1.4 Cloudlab Information
- **Profile**: [Cloudlab TAO Profile](https://www.cloudlab.us/p/EECSE6894/alex-tao)
- **Image Disk**: `urn:publicid:IDN+utah.cloudlab.us+image+eecse6894-PG0:alex-tao-final:0`

---

### 2. Artifact Checklist

* **Algorithm:** We redesigned the baseline's architecture by separating clients, servers, and DB interfaces, creating router mechanisms, deploying specialized threads to deal with read, read_txn, write and DB quires, and implementing weak consistency for transactions.

* **Program:** Our benchmark is TAOBench (see 1.2), and we added hit rate to it. The benchmark (testing logic) is integrated with our code, so there is no need to download it. The size of our repo is ~10MB. But it's recommended to have 32 cores (single machine) / 16 cores (double machines) and 25G memory for evaluation.

* **Compilation:** A compiler that supports C++17 is recommended.

* **Transformations:** Not required.

* **Binary:** Binaries are ./taobench and ./router/router, you should follow part 3 to generate and run them. 

* **Model:** Not required.

* **Data set:** Not required.

* **Run-time environment:** The recommended OS is Ubuntu 18.04, with C++ libraries for MySQL, ZeroMQ, and Memcached. Our cloudlab images already had all dependencies installed. We need root access to run taobench and modify MySQL.

* **Hardware:** 32 cores and 25G memory. You can use cloudlab c6525-100G or c6525-25G and boost with our image.

* **Run-time state:** Not required.

* **Execution:** You should make sure the ports (typically 6000 - 8000) are not blocked by firewall and MySQL can be accessed remotely. See our guide for MySQL configuration in part3. The image should have setup these. It will take about 3 min to run a regular test.

* **Metrics:** Throughputs (total operations), latencies for different operations, and cache hit rate will be evaluated.

* **Output:** Sample outputs will be explained in part4.

* **Experiments:** Experienments requires resetting MySQL, loading data into MySQL, starting router, and running taobench. If you run in a distributed way, you'll need to start client, cache1, cache2, router, and db interface separately. Please refer to part3 for instructions.

* **Disk Space:** 16 GB disk space is enough.

* **Time to Prepare:** The environment is configured in our image. If everything goes smoothly, you can finish the preparation within 15 minutes.

* **Time to Experiment:** You may need several minutes to compile and start each component of the system. The time for experiment itself is 3 minutes.

* **Publicly available?:** Our github repo and disk image on cloudlab are available, but we didn't publish our project through other sources, e.g. websites. Please contact us if you have problem accessing our code.

* **Code licenses:** Not specified.

* **Workflow frameworks used?** Not used.

* **Archived?:** Not applicable.

### 3. How to Use TAOBench

#### 3.1 Cloudlab Configuration 

To configure this artifact on Cloudlab, please follow these steps:

1. Use the "alex-tao" profile and select the OS image "final tao bench" as described in 1.4.
2. Set the hardware to `cloudlab-utah -> c6525-100g`.
3. Follow the Cloudlab instructions to initiate the node.
4. SSH into the node. The artifact files are located in `/local/taobench/`.

#### 3.2 File Structure

To use this artifact, navigate to `/local/taobench/`. Key files include:

- **`./src`**: Contains all source code for TAOBench.
  - **`benchmark.cc`**: Main function file for TAOBench:
    - **Command-Line Parsing**: Configures benchmark properties like thread count, database name, and experiment settings.
    - **Operations Tracking**: Tracks completed, failed, and overtime operations.
    - **Workload Functions**:
      - **`RunTransactions`**: Executes transactions based on experimental settings.
      - **`RunBatchInsert`**: Performs batch data insertions with multiple threads.
      - **`RunTestWorkload`**: Runs a simple workload for testing.
    - **`./mysqldb`**: Contains MySQL database configuration files.
  - **`workload_o.json`**: Represents a read-heavy workload configuration file.
  - **`db_wrapper.h`**: The interfaces to access databases or cache layers and handle scalar or transaction operations.

- **`./experiments.txt`**: Allows parameter setup for experiments, including `num_threads`, `warmup_len`, and `exp_len`.
- **`./router`**: Contains the source code for router, which should be compiled separately.
- **`./memcache`**: The cache layer implemented by us, including memcache, data structures, cache logic (memcache wrapper), and ZeroMQ utilities.

#### 3.3 Test the System

The environment is pre-configured in our disk image. Although we have setup MySQL, you may still check this document when facing problems. [MySQL Setup](https://docs.google.com/document/d/19IRzfTIO189Ok2O2dUSjod5KpCgsb518sCiPbDKDaHs). The MySQL database, named `benchmark`, can be accessed using `sudo mysql`. Each time before running experienments, you should clear all items from `objects` and `edges` tables under `benchmark`. Enter `/local/taobench/` and run `cmake . -DWITH_MYSQL=ON -DWITH_MEMCACHE=ON -DCACHE_ID=-1` to generate Makefile. Then, use the `make` command to compile, which should generate `./taobench`. Enter `/local/taobench/router` and run `g++ -O3 -DCACHE_ID=-1 -o router run.cc -pthread -lzmq -I../src` to compile router.

**Load Data Phase**:  
To load data, use the following command:

```bash
sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -load -n 150000
```

This command will load data into the benchmark MySQL database. After data loading, verify the row counts with:

SELECT COUNT(\*) FROM edges; (Expected: 150,000 rows in the edges table)
SELECT COUNT(\*) FROM objects; (Expected: 300,000 rows in the objects table)

**Run Experiments Phase**:  
To run the experiment in a direct, single-machine way, start the router first in one terminal:

```bash
sudo ./router/router
```

And run the system in another:

```bash
sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -run -e experiments.txt
```

The command above uses the parameters from [experiments.txt](experiments.txt), which includes:
```
48: Number of threads
10: Warmup length in seconds
150: Experiment length in seconds
```
After running the experiment, results will be displayed in the terminal and recorded in [/local/taobench/results.txt](results.txt).

#### 3.4 Distributed Tests

To run the experiment in a distributed way, you need at least 2 machines (machine1 for two cache servers) and machine2 for clients, routers, and DB interfaces.

First of all, you should set up MySQL and clear `objects` and `edges` tables on machine2 in the same way as 3.3. Then you'll need to modify `mysqldb/mysql_db.properties` on both machines by changing line 2 `mysqldb.url=localhost` into `mysqldb.url=<host of machine2>`, e.g. 128.110.219.42. **This is super important.** After that, you can run this command on machine1 or machine2 to load the data:
```bash
sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -run -e experiments.txt
```
If no error is thrown, it means MySQL can be accessed correctly.

**Step1:** Start the router on machine2. Enter `/local/taobench/router` and run `g++ -O3 -DCACHE_ID=-1 -o router run.cc -pthread -lzmq -I../src` to compile router. Run `./router <host of machine2> <host of machine1>` to start the router. If successful, you'll see outputs like this:
```
ZeroMQ listening on *:6001
ZeroMQ listening on *:6002
ZeroMQ listening on *:6003
ZeroMQ connecting to 128.110.219.18:6100
ZeroMQ connecting to 128.110.219.18:6102
...
```

**Step2:** Start DB interface on machine2. Compile the system with `cmake . -DWITH_MYSQL=ON -DWITH_MEMCACHE=ON -DCACHE_ID=-1` and `make` on machine2. Run `sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -run -e experiments.txt -mode backend -self-addr <host of machine2> -server-type db`. If successful, you'll see outputs like this:
```
...
ZeroMQ listening on 128.110.219.42:6401
ZeroMQ listening on 128.110.219.42:6400
ZeroMQ listening on 128.110.219.42:6500
ZeroMQ publish thro 128.110.219.42:6600
broadcast invalid 0 scala and 0 txn
broadcast invalid 0 scala and 0 txn
...
```

**Step3:** Start cache0 on machine1. Compile the system with `cmake . -DWITH_MYSQL=ON -DWITH_MEMCACHE=ON -DCACHE_ID=0` and `make` on machine1. Run `sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -run -e experiments.txt -mode backend -self-addr <host of machine1> -db-addr <host of machine2> -server-type cache`. If successful, you'll see outputs like this:
```
...
ZeroMQ listening on 128.110.219.18:6202
ZeroMQ listening on 128.110.219.18:6107
ZeroMQ listening on 128.110.219.18:6303
ZeroMQ listening on 128.110.219.18:6105
...
```

**Step4:** Start cache1 on machine1. Compile the system with `cmake . -DWITH_MYSQL=ON -DWITH_MEMCACHE=ON -DCACHE_ID=1` and `make` on machine1. Run the same command in another terminal as step3 to start it and you'll see similar outputs.

**Step5:** Start clients on machine2. Compile the system with `cmake . -DWITH_MYSQL=ON -DWITH_MEMCACHE=ON -DCACHE_ID=-1` and `make` on machine2. Run `sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -run -e experiments.txt -mode frontend -host <host of machine2> -self-addr <host of machine2> -send-rate 0.1` and wait for about 3 minutes until the final output like:
```
Cache Hit Rate: 0.908902
22703079 operations; [INSERT: Count=17991 Max=12323.52 Min=88.58 Avg=558.00] [READ: Count=22014325 Max=14338.29 Min=25.16 Avg=211.03] [UPDATE: Count=32115 Max=11956.92 Min=110.62 Avg=660.94] [READTRANSACTION: Count=635997 Max=169687.88 Min=94.18 Avg=2839.90] [WRITETRANSACTION: Count=2651 Max=15413.49 Min=255.60 Avg=1597.97] [WRITE: Count=50106 Max=12323.52 Min=88.58 Avg=623.98]
```
Note that `-send-rate` is to adjust the sending speed of clients when we want to explore latency under different throughputs. Clients usually generate requests faster than cache servers can handle, resulting in a full workload and a long latency. Note that the speed of request generation is affected by different client machines. In our cases,  send-rate 0.015 creates a full workload, and send-rate 0.0066 creates a half workload.

Explanation of running args of taobench: We added 5 parameters to the taobench executable, namely “-mode”, “-host”, "-server-type", "-db-addr" and “-self-addr”. For other parameters, please refer to the original TAOBench. “-mode” is to specify if you wanna run the “frontend”, “backend”, or both (“mix”). “-host” is for the ip address of router when you run in the “frontend” or “mix” mode. "-server-type" is to specify whether you want to run the "cache" layer or "db" layer alone when running the backend (run both of them by default). "-db-addr" is to specify the host of db machine when "-server-type" is "cache". “-self-addr” is the address of the machine on which taobench runs on. Theoretically, you can distribute clients, routers, multiple cache servers, and db interfaces onto more than 2 machines by specifying these arguments.

Explanation of running args of router: `router <self-addr> <host-addr>`. Self-addr is the address of router's machine and host-addr is the cache server's machine.

### 4. Result Information

When you check the [/local/taobench/results.txt](results.txt), you can refer to final part of the result which contains like
```
Experiment description: 48 threads, 10 seconds (warmup), 150 seconds (experiment)
Total runtime (sec): 150.011
Runtime excluding warmup (sec): 140.01
Total completed operations excluding warmup: 22703079
Throughput excluding warmup: 162153
Number of overtime operations: 24298608
Number of failed operations: 0
22703079 operations; [INSERT: Count=17991 Max=12323.52 Min=88.58 Avg=558.00] [READ: Count=22014325 Max=14338.29 Min=25.16 Avg=211.03] [UPDATE: Count=32115 Max=11956.92 Min=110.62 Avg=660.94] [READTRANSACTION: Count=635997 Max=169687.88 Min=94.18 Avg=2839.90] [WRITETRANSACTION: Count=2651 Max=15413.49 Min=255.60 Avg=1597.97] [WRITE: Count=50106 Max=12323.52 Min=88.58 Avg=623.98]
```

After done this execution, you can find slight difference in mysql database compared to original state.
Beacause the real workload of TAO is read-heavy, but there is still a few of writes.

```
mysql> SELECT COUNT(*) FROM objects;
+----------+
| COUNT(*) |
+----------+
|  3024351 |
+----------+

mysql> SELECT COUNT(*) FROM edges;
+----------+
| COUNT(*) |
+----------+
|  1500115 |
+----------+
```

> A few clarifications:

> - For throughput, each read/write/read transaction/write transaction counts as a single completed operation.
> - The last line describes operation latencies. The "Count" is the number of completed operations. The "Max", "Min", and "Avg" are latencies in microseconds. The WRITE operation category is an aggregate of inserts/updates/deletes.
> - `Overtime operations` aren't particularly meaningful anymore.
