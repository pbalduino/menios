FROM centos:latest

ADD cppcheck.repo /etc/yum.repos.d/
ADD nasm.repo /etc/yum.repos.d/

RUN yum makecache && yum groupinstall "Development Tools" -y && yum install nasm cppcheck -y
