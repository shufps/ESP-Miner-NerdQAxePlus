import { Component, OnInit, Input } from '@angular/core';
import { NbDialogRef } from '@nebular/theme';

@Component({
  selector: 'help',
  templateUrl: './help.component.html',
  styleUrls: ['./help.component.scss']
})
export class HelpComponent implements OnInit {

  key: string ="APPLICATION_PARAMETERS.HELP.append_voice_message";
  constructor(protected ref: NbDialogRef<HelpComponent> ) {

   }

   dismiss() {
    this.ref.close();
  }

  ngOnInit() {
  }

}
