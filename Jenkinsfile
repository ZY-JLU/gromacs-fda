#!groovy

pipeline {
  agent none
  stages {
    stage('Build') {
      agent {
        docker {
          image 'bernddoser/docker-devel-cpp:ubuntu-16.04-gcc-4.9-tools-1'
          label 'docker-nodes'
        }
      }
      steps {
        sh '''
          mkdir -p build
          cd build
          cmake -DCMAKE_BUILD_TYPE=release \
                -DCMAKE_C_COMPILER=/usr/bin/gcc \
                -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
                -DGMX_BUILD_MDRUN_ONLY=OFF \
                -DGMX_BUILD_FDA=ON \
                -DGMX_DEFAULT_SUFFIX=OFF \
                -DGMX_BINARY_SUFFIX=_fda \
                -DGMX_SIMD=NONE \
                -DGMX_BUILD_UNITTESTS=ON \
                -DGMX_BUILD_OWN_FFTW=ON \
                ..
          make
        '''
      }
    }
    stage('Test') {
      agent {
        docker {
          image 'bernddoser/docker-devel-cpp:ubuntu-16.04-gcc-4.9-tools-1'
          label 'docker-nodes'
        }
      }
      steps {
        script {
          try {
            sh 'cd build; make check'
          } catch (err) {
            echo "Failed: ${err}"
          } finally {
            step([
              $class: 'XUnitBuilder',
              thresholds: [[$class: 'FailedThreshold', unstableThreshold: '1']],
              tools: [[$class: 'GoogleTestType', pattern: 'build/Testing/Temporary/*.xml']]
            ])
          }
        }
      }
    }
    stage('Doxygen') {
      agent {
        docker {
          image 'bernddoser/docker-devel-cpp:ubuntu-16.04-cmake-3.7.2-doxygen-1.8.13'
          label 'docker-nodes'
        }
      }
      steps {
        sh 'cd build; make doxygen-all'
      }
    }
  }
  post {
    always {
      publishHTML( target: [
        allowMissing: false,
        alwaysLinkToLastBuild: false,
        keepAll: true,
        reportName: 'Doxygen',
        reportDir: 'build/docs/html/doxygen/html-full',
        reportFiles: 'index.xhtml'
      ])
      //deleteDir()
    }
    success {
      archiveArtifacts artifacts: 'build/bin/gmx_fda', fingerprint: true
    }
//    failure {
//      mail to:'bernd.doser@h-its.org', subject:"FAILURE: ${currentBuild.fullDisplayName}", body: "Boo, we failed."
//    }
  }
}
