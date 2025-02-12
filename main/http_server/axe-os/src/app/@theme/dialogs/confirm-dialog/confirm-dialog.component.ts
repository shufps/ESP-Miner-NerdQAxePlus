import { Component, OnInit } from '@angular/core';
import { NbDialogRef } from '@nebular/theme';

@Component({
  selector: 'app-confirm-dialog',
  templateUrl: './confirm-dialog.component.html',
  styleUrls: ['./confirm-dialog.component.scss']
})
export class ConfirmDialogComponent implements OnInit {

  key: string ="Sei sicuro di voler cancellare?";
  timeout: number = 0;
  timeoutHandler : any = null;
  btn_dx_status = "danger";
  btn_dx_text = "cancella";
  btn_dx_icon = "trash"

  btn_sx_status="warning";
  btn_sx_text="indietro";
  btn_sx_icon="circle-chevron-left"

  constructor(protected ref: NbDialogRef<ConfirmDialogComponent>) { }

  ngOnInit(): void {
    if(this.timeout > 0){

      this.timeoutHandler = setInterval(()=>{
        this.timeout--;
        if(this.timeout < 0){
          clearInterval(this.timeoutHandler)
        }
      },1000)
    }
  }

  dismiss() {
    this.ref.close();
  }

  discard() {
    this.ref.close(false);
  }

  success() {
    this.ref.close(true);
  }

}
