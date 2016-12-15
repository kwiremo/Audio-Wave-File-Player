Imports System.Threading

'Flags used in this projec
'GETSO Flag sent to request the list of songs.
'PLAYS Flag sent to start playing
'PAUSE Flag sent to pause the song.

Public Class Form1

    Dim GUIDisabled As Boolean = True

    Dim Thread_0 As New Threading.Thread(AddressOf Thread_0_method)
    Delegate Sub ComboBoxDelegate(ByVal myStr As String)
    Dim ComboBoxDel As New ComboBoxDelegate(AddressOf ComboBoxDelMethod)

    Private Sub Form1_Load(sender As Object, e As EventArgs) Handles MyBase.Load
        DisableGUI()
        
        'Open Serial Port
        Try
            SerialPort1.Open()
        Catch
            Console.WriteLine("Failed to open serial port")
        End Try
    End Sub

    Private Sub ComboBox1_SelectedIndexChanged(sender As Object, e As EventArgs) Handles ComboBox1.SelectedIndexChanged

    End Sub

    Private Sub btn_displaySong_Click(sender As Object, e As EventArgs) Handles btn_displaySong.Click


        'If thread 0 is not alive, start the thread. else ignore.
        If (Thread_0.IsAlive = False) Then
            Thread_0.Start()
            'ComboBox1.Items.Add("Thread started")
        End If

        'Send display flag.
        If SerialPort1.IsOpen Then
            SerialPort1.Write("GETSO", 0, 5)
        End If

    End Sub

    Private Sub btn_previous_Click(sender As Object, e As EventArgs) Handles btn_previous.Click
        If SerialPort1.IsOpen Then
            If (ComboBox1.SelectedIndex > 0) Then
                ComboBox1.SelectedIndex = ComboBox1.SelectedIndex - 1
                SerialPort1.Write("PLAYS", 0, 5)
                SerialPort1.Write(ComboBox1.SelectedItem.ToString())
                SerialPort1.Write(vbNullChar)

            End If
        End If
    End Sub

    Private Sub btn_next_Click(sender As Object, e As EventArgs) Handles btn_next.Click
        If SerialPort1.IsOpen Then
            'If not the last song, increment to the next song.
            If (ComboBox1.SelectedIndex <> ComboBox1.Items.Count() - 1) Then
                ComboBox1.SelectedIndex = ComboBox1.SelectedIndex + 1
                SerialPort1.Write("PLAYS", 0, 5)
                Thread.Sleep(300) 'I added this delay to see if I could get next to play
                SerialPort1.Write(ComboBox1.SelectedItem.ToString())
                SerialPort1.Write(vbNullChar)
            Else
                ComboBox1.SelectedIndex = 0
            End If
        End If
    End Sub

    Private Sub btn_play_Click(sender As Object, e As EventArgs) Handles btn_play.Click
        'Send Play flag.
        If SerialPort1.IsOpen Then
            If (ComboBox1.SelectedItem <> Nothing) Then
                SerialPort1.Write("PLAYS", 0, 5)
                Thread.Sleep(100)
                SerialPort1.Write(ComboBox1.SelectedItem.ToString())
                SerialPort1.Write(vbNullChar)

            End If
        End If
    End Sub

    Private Sub btn_pause_Click(sender As Object, e As EventArgs) Handles btn_pause.Click
        'Send pause flag.
        If SerialPort1.IsOpen Then
            SerialPort1.Write("PAUSE", 0, 5)
            btn_previous.Enabled = Not btn_previous.Enabled
            btn_next.Enabled = Not btn_next.Enabled
            btn_play.Enabled = Not btn_play.Enabled
        End If
    End Sub

    ' Visual Basic
    Private Sub SetCancelButton(ByVal myCancelBtn As Button)
        Me.CancelButton = myCancelBtn
    End Sub

    Private Sub btn_exit_Click(sender As Object, e As EventArgs) Handles btn_exit.Click
        btn_displaySong.Enabled = True
        btn_previous.Enabled = False
        btn_next.Enabled = False
        btn_play.Enabled = False
        btn_pause.Enabled = False
        ComboBox1.Items.Clear()

        If SerialPort1.IsOpen Then
            SerialPort1.Write("EXITS", 0, 5)
        End If

    End Sub

    Private Sub Thread_0_method()
        Dim str As String
        While 1
            If (SerialPort1.IsOpen) Then
                str = SerialPort1.ReadLine()
                ComboBox1.Invoke(ComboBoxDel, str)
            End If
        End While

    End Sub

    Public Sub ComboBoxDelMethod(ByVal myStr As String)
        If (GUIDisabled = True) Then
            EnableGUI()
            GUIDisabled = False
        End If

        ComboBox1.Items.Add(myStr)
    End Sub

    Private Sub EnableGUI()
        btn_displaySong.Enabled = False
        btn_previous.Enabled = True
        btn_next.Enabled = True
        btn_play.Enabled = True
        btn_pause.Enabled = True
    End Sub

    Private Sub DisableGUI()
        btn_previous.Enabled = False
        btn_next.Enabled = False
        btn_play.Enabled = False
        btn_pause.Enabled = False
    End Sub
End Class
