use redb::{Database, TableDefinition, Error, ReadableDatabase, ReadableTable}; // 核心修复：引入 ReadableTable
use std::time::{SystemTime, UNIX_EPOCH};

// 定义表结构：节点名 -> 负载值
const NODE_METRICS: TableDefinition<&str, u64> = TableDefinition::new("node_metrics");

fn main() -> Result<(), Error> {
    let db = Database::create("production_sim.redb")?;

    // --- 1. 模拟生产数据写入 (500 条) ---
    let write_txn = db.begin_write()?;
    {
        let mut table = write_txn.open_table(NODE_METRICS)?;
        
        for i in 1..=500 {
            let node_id = format!("node-{:03}", i);
            let load = (i * 7) % 101; // 模拟负载百分比
            table.insert(node_id.as_str(), &load)?;
        }
    }
    write_txn.commit()?;
    println!("成功初始化 500 个节点的监控指标。");

    // --- 2. 模拟生产查询：扫描报警节点 ---
    println!("\n--- 正在扫描高负载节点 (>90%) ---");
    let read_txn = db.begin_read()?;
    let table = read_txn.open_table(NODE_METRICS)?;

    let mut alert_count = 0;
    
    // 显式标注 result 的类型来解决 E0282
    // 在 redb 中，迭代器返回的是 (AccessGuard<K>, AccessGuard<V>)
    for result in table.iter()? {
        let (id_guard, load_guard) = result?;
        let id: &str = id_guard.value();
        let load: u64 = load_guard.value();
        
        if load > 90 {
            println!("警报：节点 {} 当前负载过高: {}%", id, load);
            alert_count += 1;
        }
    }
    
    println!("--- 扫描结束，共发现 {} 个异常节点 ---", alert_count);

    // --- 3. 模拟单点精确查询 ---
    let test_node = "node-256";
    if let Some(load_guard) = table.get(test_node)? {
        let val: u64 = load_guard.value();
        println!("\n特定节点查询 [{}]: 当前负载为 {}%", test_node, val);
    }

    Ok(())
}