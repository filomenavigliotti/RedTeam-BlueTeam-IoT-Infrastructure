# Advanced Attack Simulation and Detection in an IoT-Integrated Enterprise Infrastructure

This project was developed for the **Network Security** course (Academic Year 2025/2026) at the **University of Naples Federico II**.

The objective of this project is to simulate a realistic enterprise environment, inspired by the typical vulnerabilities of SMEs, to demonstrate how seemingly minor security flaws can lead to a systemic compromise. The project analyzes the entire **Cyber Kill Chain** and implements a proactive defensive posture using open-source monitoring systems.

<img width="2503" height="4096" alt="architettura_figa_2" src="https://github.com/user-attachments/assets/e7c67b2a-362a-41c4-92c9-9d068c111f80" />


---

## 🏛️ System Architecture

The infrastructure is divided into the following logical nodes:
*   **Attacker**: A Kali Linux-based workstation located in the external network.
*   **Edge Firewall**: An Ubuntu server managing packet filtering via `iptables`.
*   **Victim Server**: An Ubuntu Server hosting a Docker container with a vulnerable Apache Tomcat instance.
*   **Defensive Probe**: A dedicated system running a combination of **SNORT** (NIDS) and a honeypot (**Pentbox**).
*   **IoT Device**: An **Arduino Portenta H7** board responsible for the physical monitoring (motion and acoustic sensors) of the server room.
*   **SIEM**: A **Wazuh** server for centralized log collection, correlation, and analysis.

---

## 🔴 Red Team: Attack Simulation

The attack simulation unfolds through the following phases:
1.  **External Reconnaissance**: Network scanning and identification of the exposed Tomcat service on port `8080`.
2.  **Exploitation**: Remote Code Execution (RCE) achieved by deploying a malicious JSP web shell (packaged as a `.war` archive).
3.  **Privilege Escalation & Docker Escape**: Exploiting the container's root privileges to mount the host partition (`/dev/sda2`) and escaping to the host system via `chroot`.
4.  **Lateral Movement**: Mapping the internal flat network and executing an ARP Spoofing (Man-in-the-Middle) attack against the IoT device.
5.  **Persistence**: Establishing a silent backdoor by injecting ED25519 SSH cryptographic keys.

---

## 🔵 Blue Team: Defense Strategy

The resilience strategy relies on the convergence of multiple technologies:
*   **SNORT**: Detection of anomalous network patterns, port scanning, and Layer 2 attacks such as ARP poisoning.
*   **Pentbox**: A digital trap configured to simulate vulnerable services, designed to divert the attacker's attention and log their interactions.
*   **Wazuh**: Centralized SIEM for log collection, File Integrity Monitoring (FIM), and the generation of critical alerts based on the **MITRE ATT&CK** framework.
*   **IoT Security**: The Arduino microcontroller analyzes incoming traffic (anti-DoS mechanism) and forwards `Syslog` events to the SIEM for every detected physical intrusion.

---

## 🛠️ Technologies & Tools

*   **Operating Systems**: Ubuntu Server, Kali Linux
*   **Containerization**: Docker
*   **Security & Monitoring**: Snort, Wazuh, Pentbox, Nmap, John the Ripper, iptables
*   **Hardware & IoT**: Arduino Portenta H7, sensor components (motion, acoustic)
*   **Languages & Protocols**: JSP, Bash, SSH, ARP, Syslog
