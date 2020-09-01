FROM centos:latest

ADD nasm.repo /etc/yum.repos.d/
RUN yum makecache && yum groupinstall "Development Tools" -y && yum install nasm -y
