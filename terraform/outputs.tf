output "leader_public_ip" {
  description = "Public IP of leader node"
  value       = aws_instance.kv_leader.public_ip
}

output "leader_private_ip" {
  description = "Private IP of leader node"
  value       = aws_instance.kv_leader.private_ip
}

output "follower1_public_ip" {
  description = "Public IP of follower 1"
  value       = aws_instance.kv_follower1.public_ip
}

output "follower1_private_ip" {
  description = "Private IP of follower 1"
  value       = aws_instance.kv_follower1.private_ip
}

output "follower2_public_ip" {
  description = "Public IP of follower 2"
  value       = aws_instance.kv_follower2.public_ip
}

output "follower2_private_ip" {
  description = "Private IP of follower 2"
  value       = aws_instance.kv_follower2.private_ip
}

output "leader_endpoint" {
  description = "Leader client endpoint"
  value       = "http://${aws_instance.kv_leader.public_ip}:9000"
}

output "ssh_commands" {
  description = "SSH commands to connect to instances"
  value = {
    leader    = "ssh -i ~/.ssh/kv-store ubuntu@${aws_instance.kv_leader.public_ip}"
    follower1 = "ssh -i ~/.ssh/kv-store ubuntu@${aws_instance.kv_follower1.public_ip}"
    follower2 = "ssh -i ~/.ssh/kv-store ubuntu@${aws_instance.kv_follower2.public_ip}"
  }
}

output "test_commands" {
  description = "Commands to test the cluster"
  value = {
    ping   = "echo 'PING' | nc ${aws_instance.kv_leader.public_ip} 9000"
    put    = "echo 'PUT mykey myvalue' | nc ${aws_instance.kv_leader.public_ip} 9000"
    get    = "echo 'GET mykey' | nc ${aws_instance.kv_leader.public_ip} 9000"
    status = "echo 'STATUS' | nc ${aws_instance.kv_leader.public_ip} 9000"
  }
}
