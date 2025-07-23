# Multi-Threaded HTTP Proxy Server with Cache

A C-based multi-threaded HTTP proxy server with integrated LRU caching. This project demonstrates core system programming concepts like POSIX threading, semaphores, and cache management to reduce network latency and enhance performance.

---

## 📌 Features

- Multi-threaded request handling using **POSIX threads**
- Thread-safe **LRU cache** implementation using linked list and hash map
- Parses and forwards **HTTP GET** requests
- Serves cached responses to reduce repeated network calls
- Synchronization using **semaphores** for safe cache access

---

## 🎯 Project Motivation

This project was built to gain hands-on experience with:
- Networking fundamentals and how proxies handle HTTP traffic
- Managing **concurrency** and synchronization
- Understanding **cache design**, especially LRU policy
- Simulating real-world use cases such as:
  - Reducing backend load
  - Improving response time for repeated requests

---

## 🔧 Technologies & OS Concepts Used

- C programming
- POSIX threads (`pthread`)
- Semaphores (`sem_init`, `sem_wait`, `sem_post`)
- HTTP request parsing
- LRU caching logic
- Thread synchronization and resource locking

---

## ⚠️ Limitations

- Fixed-size cache: large websites may not be fully cached
- Only supports **GET** requests (no POST or HTTPS parsing)
- Some sites load multiple internal requests (e.g., images/scripts), which may not be fully cached, causing incomplete rendering

---

## 🚀 How to Run

> ⚠️ **This code runs only on Linux. Please disable browser caching before testing.**

1. **Clone the repository**
   ```bash
   git clone https://github.com/Sezan07/Multi-Threaded-Proxy-Server-with-Cache.git
   cd Multi-Threaded-Proxy-Server-with-Cache

	2.	Build the project

make all


	3.	Run the proxy server

./proxy <port_number>


	4.	Use the proxy via browser

http://localhost:<port_number>/https://www.cs.princeton.edu/



⸻

🧪 Demo
	•	First-time request → Cache Miss → Fetched from the original server
	•	Second-time request → Cache Hit → Served directly from the local LRU cache

⸻

🔮 Future Enhancements
	•	Add support for other HTTP methods (e.g., POST)
	•	Make cache size dynamic or add compression
	•	Log request/response details for debugging
	•	Add domain-based filtering or access rules

⸻

🤝 Contributing

Feel free to fork this repo, improve the functionality, and submit a pull request.
You can check the Future Enhancements section for contribution ideas.

⸻

📄 License

This project is open-source and available under the MIT License.

⸻

👨‍💻 Author

Sezan Agvan
GitHub | Bhavnagar, Gujarat, India

⸻

💬 Note

The code is well-commented for easy understanding.
For any issues or suggestions, feel free to open an issue or contribute directly.

⸻


Let me know if you want:
- A `LICENSE` file generated
- Screenshots or terminal output included
- A Markdown badge layout (for GitHub profile aesthetics)
