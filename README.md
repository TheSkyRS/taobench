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

* **Publicly available?:** Our github repo and disk image on cloudlab are available, but we didn't publish our sources through other sources, e.g. websites. Please contact us if you have problem accessing our code.

* **Code licenses:** Not specified.

* **Workflow frameworks used?** Not used.

* **Archived?:** Not applicable.

### 3. How to Use TAOBench

#### 3.1 Cloudlab Implementation

To implement this artifact on Cloudlab, please follow these steps:

1. Use the "TAO" profile and select the OS image: `urn:publicid:IDN+utah.cloudlab.us+image+eecse6894-PG0:TAO`.
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

- **`./experiments.txt`**: Allows parameter setup for experiments, including `num_threads`, `warmup_len`, and `exp_len`.

#### 3.3 Test the System

This image is fully implemented, so the environment is pre-configured. Enter `/local/taobench/` and use the `make` command to ensure any source code modifications are correctly compiled. The MySQL database, named `benchmark`, can be accessed using `sudo mysql`.

**Load Data Phase**:  
To load data, use the following command:

```bash
sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -load -n 1500000
```

This command will load data into the benchmark MySQL database. After data loading, verify the row counts with:

SELECT COUNT(*) FROM edges; (Expected: 1,500,000 rows in the edges table)
SELECT COUNT(*) FROM objects; (Expected: 3,000,000 rows in the objects table)

**Run Experiments Phase**:  
To run the experiment, use the following command:

```bash
sudo ./taobench -load-threads 48 -db mysql -p mysqldb/mysql_db.properties -c src/workload_o.json -run -e experiments.txt
```

This command loads the parameters from [experiments.txt](experiments.txt), which includes:
```
48: Number of threads
10: Warmup length in seconds
150: Experiment length in seconds
```
After running the experiment, results will be displayed in the terminal and recorded in [/local/taobench/results.txt](results.txt).

#### 3.4 Distributed Tests

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
