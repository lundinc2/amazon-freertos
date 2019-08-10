pipeline {
  agent any
  stages {
    stage('Build') {
      parallel {
        stage('Build') {
          steps {
            git(url: 'fdbgdfgb', branch: 'fdb')
            echo 'dfbdfb'
            archiveArtifacts 'dfbdf'
          }
        }
        stage('b2') {
          steps {
            timeout(time: 2) {
              sleep 3
            }

          }
        }
      }
    }
  }
}